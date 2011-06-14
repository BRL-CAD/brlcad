/* 
 * shellicon.c --
 *
 *	This is a Tk extension that adds a new element type to TkTreeCtrl.
 *	The element type's name is "shellicon". A shellicon element can
 *	display the icon for a file or folder using Win32 Shell API calls.
 *
 * Copyright (c) 2005-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

#include "tkTreeCtrl.h"
#include "tkTreeElem.h"
#include "tkWinInt.h"

#ifndef _WIN32_IE
#define _WIN32_IE 0x0501
#endif

#include <basetyps.h>
#include <commctrl.h>
#include <objbase.h>
#include <shellapi.h>

#ifndef SHGFI_ADDOVERLAYS
#if (_WIN32_IE >= 0x0500)
#define SHGFI_ADDOVERLAYS 0x000000020
#define SHGFI_OVERLAYINDEX 0x000000040
#endif
#endif

#ifndef SHIL_LARGE
#define SHIL_LARGE          0
#define SHIL_SMALL          1
#define SHIL_EXTRALARGE     2
#if 0
const GUID IID_IImageList = {0x2C247F21, 0x8591, 0x11D1,{ 0xB1, 0x6A, 0x00,0xC0, 0xF0, 0x28,0x36, 0x28} };
HRESULT (*SHGetImageListProc)(int iImageList, REFIID riid, void **ppv);
#endif
#endif

HIMAGELIST gImgListSmall = NULL;
HIMAGELIST gImgListLarge = NULL;

TreeCtrlStubs *stubs;
#define TreeCtrl_RegisterElementType(i,t) \
	stubs->TreeCtrl_RegisterElementType(i,t)
#define Tree_RedrawElement(t,i,e) \
	stubs->Tree_RedrawElement(t,i,e)
#define Tree_ElementIterateBegin(t,et) \
	stubs->Tree_ElementIterateBegin(t,et)
#define Tree_ElementIterateNext(i) \
	stubs->Tree_ElementIterateNext(i)
#define Tree_ElementIterateGet(i) \
	stubs->Tree_ElementIterateGet(i)
#define Tree_ElementIterateChanged(i,m) \
	stubs->Tree_ElementIterateChanged(i,m)
#define PerStateInfo_Free(t,ty,in) \
	stubs->PerStateInfo_Free(t,ty,in)
#define PerStateInfo_FromObj(t,pr,ty,in) \
	stubs->PerStateInfo_FromObj(t,pr,ty,in)
#define PerStateInfo_ForState(t,ty,in,st,ma) \
	stubs->PerStateInfo_ForState(t,ty,in,st,ma)
#define PerStateInfo_ObjForState(t,ty,in,st,ma) \
	stubs->PerStateInfo_ObjForState(t,ty,in,st,ma)
#define PerStateInfo_Undefine(t,ty,in,st) \
	stubs->PerStateInfo_Undefine(t,ty,in,st)
#undef pstBoolean
#define pstBoolean \
	(*stubs->TreeCtrl_pstBoolean)
#define PerStateBoolean_ForState(t,in,st,ma) \
	stubs->PerStateBoolean_ForState(t,in,st,ma)
#define PSTSave(in,sa) \
	stubs->PSTSave(in,sa)
#define PSTRestore(t,ty,in,sa) \
	stubs->PSTRestore(t,ty,in,sa)
#define TreeStateFromObj \
	stubs->TreeStateFromObj
#define BooleanCO_Init(ot,on) \
	stubs->BooleanCO_Init(ot,on)
#define StringTableCO_Init(ot,on,ta) \
	stubs->StringTableCO_Init(ot,on,ta)
#define PerStateCO_Init(a,b,c,d) \
	stubs->PerStateCO_Init(a,b,c,d)

static void AdjustForSticky(int sticky, int cavityWidth, int cavityHeight,
    int expandX, int expandY,
    int *xPtr, int *yPtr, int *widthPtr, int *heightPtr)
{
    int dx = 0;
    int dy = 0;

    if (cavityWidth > *widthPtr) {
	dx = cavityWidth - *widthPtr;
    }

    if (cavityHeight > *heightPtr) {
	dy = cavityHeight - *heightPtr;
    }

    if ((sticky & STICKY_W) && (sticky & STICKY_E)) {
	if (expandX)
	    *widthPtr += dx;
	else
	    sticky &= ~(STICKY_W | STICKY_E);
    }
    if ((sticky & STICKY_N) && (sticky & STICKY_S)) {
	if (expandY)
	    *heightPtr += dy;
	else
	    sticky &= ~(STICKY_N | STICKY_S);
    }
    if (!(sticky & STICKY_W)) {
	*xPtr += (sticky & STICKY_E) ? dx : dx / 2;
    }
    if (!(sticky & STICKY_N)) {
	*yPtr += (sticky & STICKY_S) ? dy : dy / 2;
    }
}

/* This macro gets the value of a per-state option for an element, then
 * looks for a better match from the master element if it exists */
#define OPTION_FOR_STATE(xFUNC,xTYPE,xVAR,xFIELD,xSTATE) \
    xVAR = xFUNC(tree, &elemX->xFIELD, xSTATE, &match); \
    if ((match != MATCH_EXACT) && (masterX != NULL)) { \
	xTYPE varM = xFUNC(tree, &masterX->xFIELD, xSTATE, &match2); \
	if (match2 > match) \
	    xVAR = varM; \
    }
#define BOOLEAN_FOR_STATE(xVAR,xFIELD,xSTATE) \
    OPTION_FOR_STATE(PerStateBoolean_ForState,int,xVAR,xFIELD,xSTATE)

/* This macro gets the object for a per-state option for an element, then
 * looks for a better match from the master element if it exists */
#define OBJECT_FOR_STATE(xVAR,xTYPE,xFIELD,xSTATE) \
    xVAR = PerStateInfo_ObjForState(tree, &xTYPE, &elemX->xFIELD, xSTATE, &match); \
    if ((match != MATCH_EXACT) && (masterX != NULL)) { \
	Tcl_Obj *objM = PerStateInfo_ObjForState(tree, &xTYPE, &masterX->xFIELD, xSTATE, &matchM); \
	if (matchM > match) \
	    xVAR = objM; \
    }

typedef struct ElementShellIcon ElementShellIcon;

struct ElementShellIcon
{
    TreeElement_ header;
#ifdef DEPRECATED
    PerStateInfo draw;
#endif
    Tcl_Obj *pathObj;		/* path of file or directory */
    char *path;
    Tcl_Obj *widthObj;
    int width;
    Tcl_Obj *heightObj;
    int height;
#define TYPE_DIRECTORY 0
#define TYPE_FILE 1
    int type;			/* If specified, 'path' is assumed to exist */
#define SIZE_LARGE 0
#define SIZE_SMALL 1
    int size;			/* SIZE_LARGE if unspecified */
    HIMAGELIST hImgList;	/* the system image list */
    int iIcon;			/* index into hImgList */
    /* FIXME: overlays no longer work in Win7 */
    int addOverlays;		/* only when useImgList is FALSE */
    int useImgList;		/* if false, create icons */
#define USE_SEL_ALWAYS 0	/* always draw selected icon */
#define USE_SEL_AUTO 1		/* draw selected icon when item is selected */
#define USE_SEL_NEVER 2		/* never draw the selected icon */
    int useSelected;		/* when to draw the selected icon */
    HICON hIcon;		/* icon */
    HICON hIconSel;		/* selected icon */
};

#define SHELLICON_CONF_ICON 0x0001
#define SHELLICON_CONF_SIZE 0x0002
#define SHELLICON_CONF_DRAW 0x0004

static CONST char *sizeST[] = {
    "large", "small", (char *) NULL
};
static CONST char *typeST[] = {
    "directory", "file", (char *) NULL
};
static CONST char *useSelectedST[] = {
    "always", "auto", "never", (char *) NULL
};

static Tk_OptionSpec shellIconOptionSpecs[] = {
    {TK_OPTION_CUSTOM, "-addoverlays", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementShellIcon, addOverlays),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_ICON},
#ifdef DEPRECATED
    {TK_OPTION_CUSTOM, "-draw", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementShellIcon, draw.obj),
     Tk_Offset(ElementShellIcon, draw),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_DRAW},
#endif
    {TK_OPTION_PIXELS, "-height", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementShellIcon, heightObj),
     Tk_Offset(ElementShellIcon, height),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_SIZE},
    {TK_OPTION_STRING, "-path", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementShellIcon, pathObj),
     Tk_Offset(ElementShellIcon, path),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_ICON},
    {TK_OPTION_CUSTOM, "-size",(char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementShellIcon, size),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_ICON},
    {TK_OPTION_CUSTOM, "-type", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementShellIcon, type),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_ICON},
    {TK_OPTION_CUSTOM, "-useimagelist", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementShellIcon, useImgList),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_ICON},
    {TK_OPTION_CUSTOM, "-useselected", (char *) NULL, (char *) NULL,
     (char *) NULL, -1, Tk_Offset(ElementShellIcon, useSelected),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_DRAW},
    {TK_OPTION_PIXELS, "-width", (char *) NULL, (char *) NULL,
     (char *) NULL, Tk_Offset(ElementShellIcon, widthObj),
     Tk_Offset(ElementShellIcon, width),
     TK_OPTION_NULL_OK, (ClientData) NULL, SHELLICON_CONF_SIZE},
    {TK_OPTION_END, (char *) NULL, (char *) NULL, (char *) NULL,
     (char *) NULL, 0, -1, 0, (ClientData) NULL, 0}
};

static void LoadIconIfNeeded(TreeElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    TreeElement elem = args->elem;
    ElementShellIcon *elemX = (ElementShellIcon *) elem;
    ElementShellIcon *masterX = (ElementShellIcon *) elem->master;
    SHFILEINFO sfi;
    Tcl_DString dString1, dString2;
    UINT uFlags = SHGFI_SYSICONINDEX;
    DWORD dwFileAttributes = 0;
    char *nativePath;
    int size = SIZE_LARGE;
    int overlays = 1;
    int type = -1;
    int useImgList = TRUE;
    DWORD_PTR result;

    /* -useimagelist boolean */
    if (elemX->useImgList != -1)
	useImgList = elemX->useImgList;
    else if (masterX != NULL && masterX->useImgList != -1)
	useImgList = masterX->useImgList;

    /* Not using the system image list. */
    if (!useImgList) {

	/* Already have an icon, or no path is given so no icon used */
	if ((elemX->hIcon != NULL) || (elemX->path == NULL))
	    return;

	/* Equivalent to "file nativename $path" */
	nativePath = Tcl_TranslateFileName(tree->interp, elemX->path, &dString1);
	if (nativePath == NULL)
	    return;

	/* This will be passed to system calls, so convert from UTF8 */
	nativePath = Tcl_UtfToExternalDString(NULL, nativePath, -1, &dString2);

	uFlags = SHGFI_ICON;

	/* -addoverlays boolean */
	if (elemX->addOverlays != -1)
	    overlays = elemX->addOverlays;
	else if (masterX != NULL && masterX->addOverlays != -1)
	    overlays = masterX->addOverlays;
	if (overlays)
	    uFlags |= SHGFI_ADDOVERLAYS;

	/* -size small or large */
	if (elemX->size != -1)
	    size = elemX->size;
	else if (masterX != NULL && masterX->size != -1)
	    size = masterX->size;
	switch (size) {
	    case SIZE_LARGE: uFlags |= SHGFI_LARGEICON | SHGFI_SHELLICONSIZE; break;
	    case SIZE_SMALL: uFlags |= SHGFI_SMALLICON | SHGFI_SHELLICONSIZE; break;
	}

	/* -type file or -type directory */
	if (elemX->type != -1)
	    type = elemX->type;
	else if (masterX != NULL && masterX->type != -1)
	    type = masterX->type;
	/* If SHGFI_USEFILEATTRIBUTES is set, SHGetFileInfo is supposed to
	 * assume that the file is real but not look for it on disk. This
	 * can be used to get the icon for a certain type of file, ex *.exe.
	 * In practice, lots of files get a non-generic icon when a
	 * valid file path is given. */
	if (type != -1) {
	    dwFileAttributes = (type == TYPE_FILE) ?
		FILE_ATTRIBUTE_NORMAL : FILE_ATTRIBUTE_DIRECTORY;
	    uFlags |= SHGFI_USEFILEATTRIBUTES;
	}

	/* MSDN says SHGFI_OPENICON returns the image list containing the
	 * small open icon. In practice the large open icon gets returned
	 * for SHGFI_LARGEICON, so we support it. */
	if (/*(size == SIZE_SMALL) && */(args->state & STATE_OPEN))
	    uFlags |= SHGFI_OPENICON;

	CoInitialize(NULL);

	result = SHGetFileInfo(
	    nativePath,
	    dwFileAttributes,
	    &sfi,
	    sizeof(sfi),
	    uFlags);
	if (result) {
	    elemX->hIcon = sfi.hIcon;

	    /* Remember the image list so we can get the icon size */
	    elemX->hImgList = (size == SIZE_LARGE) ? gImgListLarge : gImgListSmall;
	}

	result = SHGetFileInfo(
	    nativePath,
	    dwFileAttributes,
	    &sfi,
	    sizeof(sfi),
	    uFlags | SHGFI_SELECTED);
	if (result)
	    elemX->hIconSel = sfi.hIcon;


	CoUninitialize();

	Tcl_DStringFree(&dString1);
	Tcl_DStringFree(&dString2);

	return;
    }

    /* Using the system image list */
    if ((elemX->hImgList == NULL) && (elemX->path != NULL)) {

	/* Equivalent to "file nativename $path" */
	nativePath = Tcl_TranslateFileName(tree->interp, elemX->path, &dString1);
	if (nativePath == NULL)
	    return;

	/* This will be passed to system calls, so convert from UTF8 */
	nativePath = Tcl_UtfToExternalDString(NULL, nativePath, -1, &dString2);

	/* -size small or large */
	if (elemX->size != -1)
	    size = elemX->size;
	else if (masterX != NULL && masterX->size != -1)
	    size = masterX->size;

	switch (size) {
	    case SIZE_SMALL: uFlags |= SHGFI_SMALLICON | SHGFI_SHELLICONSIZE; break;
	    case SIZE_LARGE: uFlags |= SHGFI_LARGEICON | SHGFI_SHELLICONSIZE; break;
	}

	/* -type file or -type directory */
	if (elemX->type != -1)
	    type = elemX->type;
	else if (masterX != NULL && masterX->type != -1)
	    type = masterX->type;
	/* If SHGFI_USEFILEATTRIBUTES is set, SHGetFileInfo is supposed to
	 * assume that the file is real but not look for it on disk. This
	 * can be used to get the icon for a certain type of file, ex *.exe.
	 * In practice, lots of files get a non-generic icon when a
	 * valid file path is given. */
	if (type != -1) {
	    dwFileAttributes = (type == TYPE_FILE) ?
		FILE_ATTRIBUTE_NORMAL : FILE_ATTRIBUTE_DIRECTORY;
	    uFlags |= SHGFI_USEFILEATTRIBUTES;
	}

	/* MSDN says SHGFI_OPENICON returns the image list containing the
	 * small open icon. In practice the large open icon gets returned
	 * for SHGFI_LARGEICON. */
	if (/*(size == SIZE_SMALL) && */(args->state & STATE_OPEN))
	    uFlags |= SHGFI_OPENICON;

	CoInitialize(NULL);

	elemX->hImgList = (HIMAGELIST) SHGetFileInfo(
	    nativePath,
	    dwFileAttributes,
	    &sfi,
	    sizeof(sfi),
	    uFlags);
	if (elemX->hImgList != NULL) {
	    elemX->iIcon = sfi.iIcon;
	}

	CoUninitialize();

	Tcl_DStringFree(&dString1);
	Tcl_DStringFree(&dString2);
    }
}

static void ForgetIcon(ElementShellIcon *elemX)
{
    if (elemX->hIcon != NULL)
	DestroyIcon(elemX->hIcon);
    if (elemX->hIconSel != NULL)
	DestroyIcon(elemX->hIconSel);
    elemX->hImgList = NULL;
    elemX->hIcon = elemX->hIconSel = NULL;
}

static void DeleteProcShellIcon(TreeElementArgs *args)
{
    TreeElement elem = args->elem;
    ElementShellIcon *elemX = (ElementShellIcon *) elem;

    ForgetIcon(elemX);
}

static int WorldChangedProcShellIcon(TreeElementArgs *args)
{
    ElementShellIcon *elemX = (ElementShellIcon *) args->elem;
    int flagM = args->change.flagMaster;
    int flagS = args->change.flagSelf;
    int mask = 0;

    if ((flagS | flagM) & (SHELLICON_CONF_ICON | SHELLICON_CONF_SIZE))
	mask |= CS_DISPLAY | CS_LAYOUT;
    if ((flagS | flagM) & (SHELLICON_CONF_DRAW))
	mask |= CS_DISPLAY;

    if ((flagS | flagM) & SHELLICON_CONF_ICON) {
	ForgetIcon(elemX);
    }

    return mask;
}

static int ConfigProcShellIcon(TreeElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    TreeElement elem = args->elem;
    ElementShellIcon *elemX = (ElementShellIcon *) elem;
    Tk_SavedOptions savedOptions;
    int error;
    Tcl_Obj *errorResult = NULL;

    for (error = 0; error <= 1; error++) {
	if (error == 0) {
	    if (Tk_SetOptions(tree->interp, (char *) elemX,
			elem->typePtr->optionTable,
			args->config.objc, args->config.objv, tree->tkwin,
			&savedOptions, &args->config.flagSelf) != TCL_OK) {
		args->config.flagSelf = 0;
		continue;
	    }

	    /* xxx */

	    Tk_FreeSavedOptions(&savedOptions);
	    break;
	} else {
	    errorResult = Tcl_GetObjResult(tree->interp);
	    Tcl_IncrRefCount(errorResult);
	    Tk_RestoreSavedOptions(&savedOptions);

	    /* xxx */

	    Tcl_SetObjResult(tree->interp, errorResult);
	    Tcl_DecrRefCount(errorResult);
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

static int CreateProcShellIcon(TreeElementArgs *args)
{
    ElementShellIcon *elemX = (ElementShellIcon *) args->elem;

    elemX->type = -1;
    elemX->size = -1;
    elemX->addOverlays = -1;
    elemX->useImgList = -1;
    elemX->useSelected = -1;

    return TCL_OK;
}

static void DisplayProcShellIcon(TreeElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    TreeElement elem = args->elem;
    ElementShellIcon *elemX = (ElementShellIcon *) elem;
    ElementShellIcon *masterX = (ElementShellIcon *) elem->master;
    int state = args->state;
    int x = args->display.x, y = args->display.y;
    int width, height;
    int match, match2;
    int draw;
    HDC hDC;
    TkWinDCState dcState;
    UINT fStyle = ILD_TRANSPARENT;
    HICON hIcon = NULL;
    int useSelected;

#ifdef DEPRECATED
    BOOLEAN_FOR_STATE(draw, draw, state)
    if (!draw)
	return;
#endif

    LoadIconIfNeeded(args);

    if (elemX->hImgList == NULL)
	return;

    (void) ImageList_GetIconSize(elemX->hImgList, &width, &height);
    AdjustForSticky(args->display.sticky,
	args->display.width, args->display.height,
	FALSE, FALSE,
	&x, &y, &width, &height);

    hDC = TkWinGetDrawableDC(tree->display, args->display.drawable, &dcState);

    useSelected = elemX->useSelected;
    if (useSelected == -1 && masterX != NULL)
	useSelected = masterX->useSelected;
    switch (useSelected) {
	case USE_SEL_ALWAYS:
	    hIcon = elemX->hIconSel;
	    fStyle |= ILD_SELECTED;
	    break;
	case -1:
	case USE_SEL_AUTO:
	    hIcon = (state & STATE_SELECTED) ? elemX->hIconSel : elemX->hIcon;
	    if (state & STATE_SELECTED)
		fStyle |= ILD_SELECTED;
	    break;
	case USE_SEL_NEVER:
	    hIcon = elemX->hIcon;
	    break;
    }
    if (hIcon != NULL) {
#if 1
	/* If DI_DEFAULTSIZE is used, small icons get stretched */
	DrawIconEx(hDC, x, y, hIcon, 0, 0, FALSE, NULL, DI_IMAGE | DI_MASK /*| DI_DEFAULTSIZE*/);
#else
	/* Works fine for large, but not for small (they get stretched) */
	DrawIcon(hDC, x, y, hIcon);
#endif
    } else {
	ImageList_Draw(elemX->hImgList, elemX->iIcon, hDC, x, y, fStyle);
    }
    TkWinReleaseDrawableDC(args->display.drawable, hDC, &dcState);

    /* If this is a master element, forget the icon being used because the
     * appearance may change between items based on the open state */
    if (masterX == NULL) {
	ForgetIcon(elemX);
    }
}

static void NeededProcShellIcon(TreeElementArgs *args)
{
    TreeElement elem = args->elem;
    ElementShellIcon *elemX = (ElementShellIcon *) elem;
    ElementShellIcon *masterX = (ElementShellIcon *) elem->master;
    int width = 0, height = 0;
    int size = SIZE_LARGE;
    HIMAGELIST hImgList = NULL;

    /* The icon is not loaded until it is actually going to be displayed.
     * This is a good thing since loading the icons can take a while */
/*    LoadIconIfNeeded(args);*/

    if (elemX->hImgList != NULL) {
	hImgList = elemX->hImgList;

    } else if (elemX->path != NULL) {
	if (elemX->size != -1)
	    size = elemX->size;
	else if (masterX != NULL && masterX->size != -1)
	    size = masterX->size;
	switch (size) {
	    case SIZE_SMALL: hImgList = gImgListSmall; break;
	    case SIZE_LARGE: hImgList = gImgListLarge; break;
	}
    }

    if (hImgList != NULL) {
	(void) ImageList_GetIconSize(hImgList, &width, &height);
    }

    if (elemX->widthObj != NULL)
	width = elemX->width;
    else if ((masterX != NULL) && (masterX->widthObj != NULL))
	width = masterX->width;

    if (elemX->heightObj != NULL)
	height = elemX->height;
    else if ((masterX != NULL) && (masterX->heightObj != NULL))
	height = masterX->height;

    args->needed.width = width;
    args->needed.height = height;
}

static int StateProcShellIcon(TreeElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    TreeElement elem = args->elem;
    ElementShellIcon *elemX = (ElementShellIcon *) elem;
    ElementShellIcon *masterX = (ElementShellIcon *) elem->master;
    int match, match2;
#ifdef DEPRECATED
    int draw1, draw2;
#endif
    int sel1, sel2;
    int open1, open2;
    int mask = 0;

#ifdef DEPRECATED
    BOOLEAN_FOR_STATE(draw1, draw, args->states.state1)
    if (draw1 == -1)
	draw1 = 1;
#endif
    open1 = (args->states.state1 & STATE_OPEN) != 0;
    sel1 = (args->states.state1 & STATE_SELECTED) != 0;

#ifdef DEPRECATED
    BOOLEAN_FOR_STATE(draw2, draw, args->states.state2)
    if (draw2 == -1)
	draw2 = 1;
#endif
    open2 = (args->states.state2 & STATE_OPEN) != 0;
    sel2 = (args->states.state2 & STATE_SELECTED) != 0;

    if (elemX->path == NULL)
	open1 = open2 = sel1 = sel2 = 0;

    if (
#ifdef DEPRECATED
	(draw1 != draw2) ||
#endif
	(sel1 != sel2) || (open1 != open2))
	mask |= CS_DISPLAY;

    /* Directories may have an open and closed icon. */
    if (open1 != open2) {
	ForgetIcon(elemX);
    }

    return mask;
}

static int UndefProcShellIcon(TreeElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementShellIcon *elemX = (ElementShellIcon *) args->elem;
    int modified = 0;

#ifdef DEPRECATED
    modified |= PerStateInfo_Undefine(tree, &pstBoolean, &elemX->draw, args->state);
#endif
    return modified;
}

static int ActualProcShellIcon(TreeElementArgs *args)
{
    TreeCtrl *tree = args->tree;
    ElementShellIcon *elemX = (ElementShellIcon *) args->elem;
    ElementShellIcon *masterX = (ElementShellIcon *) args->elem->master;
    static CONST char *optionName[] = {
#ifdef DEPRECATED
	"-draw",
#endif
	(char *) NULL };
    int index, match, matchM;
    Tcl_Obj *obj = NULL;

    if (Tcl_GetIndexFromObj(tree->interp, args->actual.obj, optionName,
		"option", 0, &index) != TCL_OK)
	return TCL_ERROR;

    switch (index) {
#ifdef DEPRECATED
	case 0:
	{
	    OBJECT_FOR_STATE(obj, pstBoolean, draw, args->state)
	    break;
	}
#endif
	default:
	    break;
    }
    if (obj != NULL)
	Tcl_SetObjResult(tree->interp, obj);
    return TCL_OK;
}

TreeElementType elemTypeShellIcon = {
    "shellicon",
    sizeof(ElementShellIcon),
    shellIconOptionSpecs,
    NULL,
    CreateProcShellIcon,
    DeleteProcShellIcon,
    ConfigProcShellIcon,
    DisplayProcShellIcon,
    NeededProcShellIcon,
    NULL, /* heightProc */
    WorldChangedProcShellIcon,
    StateProcShellIcon,
    UndefProcShellIcon,
    ActualProcShellIcon,
    NULL /* onScreenProc */
};

DLLEXPORT int Shellicon_Init(Tcl_Interp *interp)
{
#if 1
    SHFILEINFO sfi;
#else
    HMODULE hLib;
#endif

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.4", 0) == NULL) {
	return TCL_ERROR;
    }
#endif
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, "8.4", 0) == NULL) {
	return TCL_ERROR;
    }
#endif

    /* InitCommonControlsEx must be called to use the ImageList functions */
    /* This is already done by Tk on NT */
    if (TkWinGetPlatformId() != VER_PLATFORM_WIN32_NT) {
	INITCOMMONCONTROLSEX comctl;
	ZeroMemory(&comctl, sizeof(comctl));
	(void) InitCommonControlsEx(&comctl);
    }

#if 1
    /* Get the sytem image lists (small and large) */
    CoInitialize(NULL);
    gImgListSmall = (HIMAGELIST) SHGetFileInfo(".exe", FILE_ATTRIBUTE_NORMAL, &sfi,
	sizeof(sfi), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
    gImgListLarge = (HIMAGELIST) SHGetFileInfo(".exe", FILE_ATTRIBUTE_NORMAL, &sfi,
	sizeof(sfi), SHGFI_USEFILEATTRIBUTES | SHGFI_SYSICONINDEX | SHGFI_LARGEICON);
    CoUninitialize();
#else
    /* FIXME: WinXP only */
    /* This is broken somewhere */
    hLib = LoadLibraryA("shell32");
    SHGetImageListProc = (FARPROC) GetProcAddress(hLib, (LPCSTR) 727);
    SHGetImageListProc(SHIL_SMALL, &IID_IImageList, &gImgListSmall);
    SHGetImageListProc(SHIL_LARGE, &IID_IImageList, &gImgListLarge);
    SHGetImageListProc(SHIL_EXTRALARGE, &IID_IImageList, &gImgListXtraLarge);
    dbwin("small %p large %p xtralarge %p\n", gImgListSmall, gImgListLarge, gImgListXtraLarge);
    FreeLibrary(hLib);
#endif

#if 0
{
typedef BOOL (WINAPI *FileIconInitProc)(BOOL fFullInit);
HMODULE hShell32 = LoadLibrary("shell32.dll");
FileIconInitProc FileIconInit = (FileIconInitProc) GetProcAddress(hShell32, (LPCSTR)660);
FileIconInit(TRUE);
}
#endif

    /* Load TkTreeCtrl */
    if (Tcl_PkgRequire(interp, "treectrl", PACKAGE_PATCHLEVEL, TRUE) == NULL)
	return TCL_ERROR;

    /* Get the stubs table from TkTreeCtrl */
    stubs = Tcl_GetAssocData(interp, "TreeCtrlStubs", NULL);
    if (stubs == NULL)
	return TCL_ERROR;

#ifdef TREECTRL_DEBUG
    if (sizeof(TreeCtrl) != stubs->sizeofTreeCtrl ||
	    sizeof(TreeCtrlStubs) != stubs->sizeofTreeCtrlStubs ||
	    sizeof(TreeElement) != stubs->sizeofTreeElement ||
	    sizeof(TreeElementArgs) != stubs->sizeofTreeElementArgs) {
	Tcl_SetResult(interp, "probably forgot to recompile shellicon",
		TCL_VOLATILE);
	return TCL_ERROR;
    }
#endif

    /* Initialize the options table */
    BooleanCO_Init(shellIconOptionSpecs, "-addoverlays");
    BooleanCO_Init(shellIconOptionSpecs, "-useimagelist");
    PerStateCO_Init(shellIconOptionSpecs, "-draw", &pstBoolean,
	    TreeStateFromObj);
    StringTableCO_Init(shellIconOptionSpecs, "-size", sizeST);
    StringTableCO_Init(shellIconOptionSpecs, "-type", typeST);
    StringTableCO_Init(shellIconOptionSpecs, "-useselected", useSelectedST);

    /* Add the "shellicon" element type */
    if (TreeCtrl_RegisterElementType(interp, &elemTypeShellIcon) != TCL_OK)
	return TCL_ERROR;

    if (Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_PATCHLEVEL) != TCL_OK) {
	return TCL_ERROR;
    }

    return TCL_OK;
}

DLLEXPORT int Shellicon_SafeInit(Tcl_Interp *interp)
{
    return Shellicon_Init(interp);
}

