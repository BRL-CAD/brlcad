/*
 * tclMacLoad.c --
 *
 *	This procedure provides a version of the TclLoadFile for use
 *	on the Macintosh.  This procedure will only work with systems 
 *	that use the Code Fragment Manager.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacLoad.c 1.20 97/11/20 18:39:20
 */

#include <CodeFragments.h>
#include <Errors.h>
#include <Resources.h>
#include <Strings.h>
#include <FSpCompat.h>

/*
 * Seems that the 3.0.1 Universal headers leave this define out.  So we
 * define it here...
 */
 
#ifndef fragNoErr
    #define fragNoErr noErr
#endif

#include "tclPort.h"
#include "tclInt.h"
#include "tclMacInt.h"

#if GENERATINGPOWERPC
    #define OUR_ARCH_TYPE kPowerPCCFragArch
#else
    #define OUR_ARCH_TYPE kMotorola68KCFragArch
#endif

/*
 * The following data structure defines the structure of a code fragment
 * resource.  We can cast the resource to be of this type to access
 * any fields we need to see.
 */
struct CfrgHeader {
    long 	res1;
    long 	res2;
    long 	version;
    long 	res3;
    long 	res4;
    long 	filler1;
    long 	filler2;
    long 	itemCount;
    char	arrayStart;	/* Array of externalItems begins here. */
};
typedef struct CfrgHeader CfrgHeader, *CfrgHeaderPtr, **CfrgHeaderPtrHand;

/*
 * The below structure defines a cfrag item within the cfrag resource.
 */
struct CfrgItem {
    OSType 	archType;
    long 	updateLevel;
    long	currVersion;
    long	oldDefVersion;
    long	appStackSize;
    short	appSubFolder;
    char	usage;
    char	location;
    long	codeOffset;
    long	codeLength;
    long	res1;
    long	res2;
    short	itemSize;
    Str255	name;		/* This is actually variable sized. */
};
typedef struct CfrgItem CfrgItem;

/*
 *----------------------------------------------------------------------
 *
 * TclLoadFile --
 *
 *	This procedure is called to carry out dynamic loading of binary
 *	code for the Macintosh.  This implementation is based on the
 *	Code Fragment Manager & will not work on other systems.
 *
 * Results:
 *	The result is TCL_ERROR, and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	New binary code is loaded.
 *
 *----------------------------------------------------------------------
 */

int
TclLoadFile(
    Tcl_Interp *interp,		/* Used for error reporting. */
    char *fileName,		/* Name of the file containing the desired
				 * code. */
    char *sym1, char *sym2,	/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr,
    Tcl_PackageInitProc **proc2Ptr)
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
{
    CFragConnectionID connID;
    Ptr dummy;
    OSErr err;
    CFragSymbolClass symClass;
    FSSpec fileSpec;
    short fragFileRef, saveFileRef;
    Handle fragResource;
    UInt32 offset = 0;
    UInt32 length = kCFragGoesToEOF;
    char packageName[255];
    Str255 errName;
    
    /*
     * First thing we must do is infer the package name from the sym1
     * variable.  This is kind of dumb since the caller actually knows
     * this value, it just doesn't give it to us.
     */
    strcpy(packageName, sym1);
    *packageName = (char) tolower(*packageName);
    packageName[strlen(packageName) - 5] = NULL;
    
    err = FSpLocationFromPath(strlen(fileName), fileName, &fileSpec);
    if (err != noErr) {
	interp->result = "could not locate shared library";
	return TCL_ERROR;
    }
    
    /*
     * See if this fragment has a 'cfrg' resource.  It will tell us were
     * to look for the fragment in the file.  If it doesn't exist we will
     * assume we have a ppc frag using the whole data fork.  If it does
     * exist we find the frag that matches the one we are looking for and
     * get the offset and size from the resource.
     */
    saveFileRef = CurResFile();
    SetResLoad(false);
    fragFileRef = FSpOpenResFile(&fileSpec, fsRdPerm);
    SetResLoad(true);
    if (fragFileRef != -1) {
	UseResFile(fragFileRef);
	fragResource = Get1Resource(kCFragResourceType, kCFragResourceID);
	HLock(fragResource);
	if (ResError() == noErr) {
	    CfrgItem* srcItem;
	    long itemCount, index;
	    Ptr itemStart;

	    itemCount = (*(CfrgHeaderPtrHand)fragResource)->itemCount;
	    itemStart = &(*(CfrgHeaderPtrHand)fragResource)->arrayStart;
	    for (index = 0; index < itemCount;
		 index++, itemStart += srcItem->itemSize) {
		srcItem = (CfrgItem*)itemStart;
		if (srcItem->archType != OUR_ARCH_TYPE) continue;
		if (!strncasecmp(packageName, (char *) srcItem->name + 1,
			srcItem->name[0])) {
		    offset = srcItem->codeOffset;
		    length = srcItem->codeLength;
		}
	    }
	}
	/*
	 * Close the resource file.  If the extension wants to reopen the
	 * resource fork it should use the tclMacLibrary.c file during it's
	 * construction.
	 */
	HUnlock(fragResource);
	ReleaseResource(fragResource);
	CloseResFile(fragFileRef);
	UseResFile(saveFileRef);
    }

    /*
     * Now we can attempt to load the fragement using the offset & length
     * obtained from the resource.  We don't worry about the main entry point
     * as we are going to search for specific entry points passed to us.
     */
    
    c2pstr(packageName);
    err = GetDiskFragment(&fileSpec, offset, length, (StringPtr) packageName,
	    kLoadCFrag, &connID, &dummy, errName);
    if (err != fragNoErr) {
	p2cstr(errName);
	Tcl_AppendResult(interp, "couldn't load file \"", fileName,
	    "\": ", errName, (char *) NULL);
	return TCL_ERROR;
    }
    
    c2pstr(sym1);
    err = FindSymbol(connID, (StringPtr) sym1, (Ptr *) proc1Ptr, &symClass);
    p2cstr((StringPtr) sym1);
    if (err != fragNoErr || symClass == kDataCFragSymbol) {
	interp->result =
	    "could not find Initialization routine in library";
	return TCL_ERROR;
    }

    c2pstr(sym2);
    err = FindSymbol(connID, (StringPtr) sym2, (Ptr *) proc2Ptr, &symClass);
    p2cstr((StringPtr) sym2);
    if (err != fragNoErr || symClass == kDataCFragSymbol) {
	*proc2Ptr = NULL;
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGuessPackageName --
 *
 *	If the "load" command is invoked without providing a package
 *	name, this procedure is invoked to try to figure it out.
 *
 * Results:
 *	Always returns 0 to indicate that we couldn't figure out a
 *	package name;  generic code will then try to guess the package
 *	from the file name.  A return value of 1 would have meant that
 *	we figured out the package name and put it in bufPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGuessPackageName(
    char *fileName,		/* Name of file containing package (already
				 * translated to local form if needed). */
    Tcl_DString *bufPtr)	/* Initialized empty dstring.  Append
				 * package name to this if possible. */
{
    return 0;
}
