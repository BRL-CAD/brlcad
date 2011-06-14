/* 
 * tkTreeElem.h --
 *
 *	This module is the header for elements in treectrl widgets.
 *
 * Copyright (c) 2002-2010 Tim Baker
 *
 * RCS: @(#) $Id$
 */

typedef struct TreeElementType TreeElementType;
typedef struct TreeElement_ TreeElement_;
typedef struct TreeElementArgs TreeElementArgs;

struct TreeElementArgs
{
    TreeCtrl *tree;
    TreeElement elem;
    int state;
    struct {
	TreeItem item;
	TreeItemColumn column;
    } create;
    struct {
	int noop;
    } delete;
    struct {
	int objc;
	Tcl_Obj *CONST *objv;
	int flagSelf;
	TreeItem item;
	TreeItemColumn column;
    } config;
    struct {
	int x;
	int y;
	int width;
	int height;
#define STICKY_W 0x1000 /* These values must match ELF_STICKY_xxx */
#define STICKY_N 0x2000
#define STICKY_E 0x4000
#define STICKY_S 0x8000
	int sticky;
	TreeDrawable td;
	Drawable drawable;
	TreeRectangle bounds;
	TreeColumn column; /* needed for gradients */
	TreeItem item; /* needed for gradients */
    } display;
    struct {
	int fixedWidth;
	int fixedHeight;
	int maxWidth;
	int maxHeight;
	int width;
	int height;
    } needed;
    struct {
	int fixedWidth;
	int height;
    } height;
    struct {
	int flagTree;
	int flagMaster;
	int flagSelf;
    } change;
    struct {
	int state1;
	int state2;
	int draw1;
	int draw2;
	int visible1;
	int visible2;
    } states;
    struct {
	Tcl_Obj *obj;
    } actual;
    struct {
	int visible;
    } screen;
};

struct TreeElementType
{
    char *name; /* "image", "text" */
    int size; /* size of an TreeElement */
    Tk_OptionSpec *optionSpecs;
    Tk_OptionTable optionTable;
    int (*createProc)(TreeElementArgs *args);
    void (*deleteProc)(TreeElementArgs *args);
    int (*configProc)(TreeElementArgs *args);
    void (*displayProc)(TreeElementArgs *args);
    void (*neededProc)(TreeElementArgs *args);
    void (*heightProc)(TreeElementArgs *args);
    int (*changeProc)(TreeElementArgs *args);
    int (*stateProc)(TreeElementArgs *args);
    int (*undefProc)(TreeElementArgs *args);
    int (*actualProc)(TreeElementArgs *args);
    void (*onScreenProc)(TreeElementArgs *args);
    TreeElementType *next;
};

/* list of these for each style */
struct TreeElement_
{
    Tk_Uid name;		/* "elem2", "eText" etc */
    TreeElementType *typePtr;
    TreeElement master;		/* NULL if this is master */
    DynamicOption *options;	/* Dynamically-allocated options. */
    /* type-specific data here */
};

MODULE_SCOPE TreeElementType treeElemTypeBitmap;
MODULE_SCOPE TreeElementType treeElemTypeBorder;
MODULE_SCOPE TreeElementType treeElemTypeCheckButton;
MODULE_SCOPE TreeElementType treeElemTypeImage;
MODULE_SCOPE TreeElementType treeElemTypeRect;
MODULE_SCOPE TreeElementType treeElemTypeText;
MODULE_SCOPE TreeElementType treeElemTypeWindow;

#define ELEMENT_TYPE_MATCHES(t1,t2) ((t1)->name == (t2)->name)

/***** ***** *****/

MODULE_SCOPE int TreeElement_GetSortData(TreeCtrl *tree, TreeElement elem, int type, long *lv, double *dv, char **sv);

typedef struct TreeIterate_ *TreeIterate;

MODULE_SCOPE int TreeElement_TypeFromObj(TreeCtrl *tree, Tcl_Obj *objPtr, TreeElementType **typePtrPtr);
MODULE_SCOPE void Tree_RedrawElement(TreeCtrl *tree, TreeItem item, TreeElement elem);
MODULE_SCOPE TreeIterate Tree_ElementIterateBegin(TreeCtrl *tree, TreeElementType *elemTypePtr);
MODULE_SCOPE TreeIterate Tree_ElementIterateNext(TreeIterate iter_);
MODULE_SCOPE TreeElement Tree_ElementIterateGet(TreeIterate iter_);
MODULE_SCOPE void Tree_ElementIterateChanged(TreeIterate iter_, int mask);
MODULE_SCOPE void Tree_ElementChangedItself(TreeCtrl *tree, TreeItem item,
    TreeItemColumn column, TreeElement elem, int flags, int mask);
MODULE_SCOPE int TreeCtrl_RegisterElementType(Tcl_Interp *interp, TreeElementType *newTypePtr);

typedef struct TreeCtrlStubs TreeCtrlStubs;
struct TreeCtrlStubs
{
#ifdef TREECTRL_DEBUG
    int sizeofTreeCtrl;
    int sizeofTreeCtrlStubs;
    int sizeofTreeElement;
    int sizeofTreeElementArgs;
#endif
    int (*TreeCtrl_RegisterElementType)(Tcl_Interp *interp,
		TreeElementType *typePtr);
    void (*Tree_RedrawElement)(TreeCtrl *tree, TreeItem item,
		TreeElement elem);
    TreeIterate (*Tree_ElementIterateBegin)(TreeCtrl *tree,
		TreeElementType *elemTypePtr);
    TreeIterate (*Tree_ElementIterateNext)(TreeIterate iter_);
    TreeElement (*Tree_ElementIterateGet)(TreeIterate iter_);
    void (*Tree_ElementIterateChanged)(TreeIterate iter_, int mask);
    void (*PerStateInfo_Free)(TreeCtrl *tree, PerStateType *typePtr,
		PerStateInfo *pInfo);
    int (*PerStateInfo_FromObj)(TreeCtrl *tree, StateFromObjProc proc,
		PerStateType *typePtr, PerStateInfo *pInfo);
    PerStateData *(*PerStateInfo_ForState)(TreeCtrl *tree,
		PerStateType *typePtr, PerStateInfo *pInfo, int state, int *match);
    Tcl_Obj *(*PerStateInfo_ObjForState)(TreeCtrl *tree,
		PerStateType *typePtr, PerStateInfo *pInfo, int state, int *match);
    int (*PerStateInfo_Undefine)(TreeCtrl *tree, PerStateType *typePtr,
		PerStateInfo *pInfo, int state);
    PerStateType *pstBoolean;
    int (*PerStateBoolean_ForState)(TreeCtrl *tree, PerStateInfo *pInfo,
		int state, int *match);
    void (*PSTSave)(PerStateInfo *pInfo, PerStateInfo *pSave);
    void (*PSTRestore)(TreeCtrl *tree, PerStateType *typePtr,
		PerStateInfo *pInfo, PerStateInfo *pSave);
    int (*TreeStateFromObj)(TreeCtrl *tree, Tcl_Obj *obj, int *stateOff,
		int *stateOn);
    int (*BooleanCO_Init)(Tk_OptionSpec *optionTable, CONST char *optionName);
    int (*StringTableCO_Init)(Tk_OptionSpec *optionTable,
		CONST char *optionName, CONST char **tablePtr);
    int (*PerStateCO_Init)(Tk_OptionSpec *optionTable, CONST char *optionName,
		PerStateType *typePtr, StateFromObjProc proc);
};

