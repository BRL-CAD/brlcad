/* 
 * tclMacFile.c --
 *
 *      This file implements the channel drivers for Macintosh
 *	files.  It also comtains Macintosh version of other Tcl
 *	functions that deal with the file system.
 *
 * Copyright (c) 1995-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacFile.c 1.57 97/04/23 16:23:05
 */

/*
 * Note: This code eventually needs to support async I/O.  In doing this
 * we will need to keep track of all current async I/O.  If exit to shell
 * is called - we shouldn't exit until all asyc I/O completes.
 */

#include "tclInt.h"
#include "tclPort.h"
#include "tclMacInt.h"
#include <Aliases.h>
#include <Errors.h>
#include <Processes.h>
#include <Strings.h>
#include <Types.h>
#include <MoreFiles.h>
#include <MoreFilesExtras.h>
#include <FSpCompat.h>

/*
 * Static variables used by the TclMacStat function.
 */
static int initalized = false;
static long gmt_offset;

/*
 * The variable below caches the name of the current working directory
 * in order to avoid repeated calls to getcwd.  The string is malloc-ed.
 * NULL means the cache needs to be refreshed.
 */

static char *currentDir =  NULL;

/*
 *----------------------------------------------------------------------
 *
 * TclChdir --
 *
 *	Change the current working directory.
 *
 * Results:
 *	The result is a standard Tcl result.  If an error occurs and 
 *	interp isn't NULL, an error message is left in interp->result.
 *
 * Side effects:
 *	The working directory for this application is changed.  Also
 *	the cache maintained used by TclGetCwd is deallocated and
 *	set to NULL.
 *
 *----------------------------------------------------------------------
 */

int
TclChdir(
    Tcl_Interp *interp,		/* If non NULL, used for error reporting. */
    char *dirName)		/* Path to new working directory. */
{
    FSSpec spec;
    OSErr err;
    Boolean isFolder;
    long dirID;

    if (currentDir != NULL) {
	ckfree(currentDir);
	currentDir = NULL;
    }

    err = FSpLocationFromPath(strlen(dirName), dirName, &spec);
    if (err != noErr) {
	errno = ENOENT;
	goto chdirError;
    }
    
    err = FSpGetDirectoryID(&spec, &dirID, &isFolder);
    if (err != noErr) {
	errno = ENOENT;
	goto chdirError;
    }

    if (isFolder != true) {
	errno = ENOTDIR;
	goto chdirError;
    }

    err = FSpSetDefaultDir(&spec);
    if (err != noErr) {
	switch (err) {
	    case afpAccessDenied:
		errno = EACCES;
		break;
	    default:
		errno = ENOENT;
	}
	goto chdirError;
    }

    return TCL_OK;
    chdirError:
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't change working directory to \"",
		dirName, "\": ", Tcl_PosixError(interp), (char *) NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetCwd --
 *
 *	Return the path name of the current working directory.
 *
 * Results:
 *	The result is the full path name of the current working
 *	directory, or NULL if an error occurred while figuring it
 *	out.  If an error occurs and interp isn't NULL, an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	The path name is cached to avoid having to recompute it
 *	on future calls;  if it is already cached, the cached
 *	value is returned.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetCwd(
    Tcl_Interp *interp)		/* If non NULL, used for error reporting. */
{
    FSSpec theSpec;
    int length;
    Handle pathHandle = NULL;
    
    if (currentDir == NULL) {
	if (FSpGetDefaultDir(&theSpec) != noErr) {
	    if (interp != NULL) {
		interp->result = "error getting working directory name";
	    }
	    return NULL;
	}
	if (FSpPathFromLocation(&theSpec, &length, &pathHandle) != noErr) {
	    if (interp != NULL) {
		interp->result = "error getting working directory name";
	    }
	    return NULL;
	}
	HLock(pathHandle);
	currentDir = (char *) ckalloc((unsigned) (length + 1));
	strcpy(currentDir, *pathHandle);
	HUnlock(pathHandle);
	DisposeHandle(pathHandle);	
    }
    return currentDir;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitPid --
 *
 *	Fakes a call to wait pid.
 *
 * Results:
 *	Always returns -1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Pid
Tcl_WaitPid(
    Tcl_Pid pid,
    int *statPtr,
    int options)
{
    return (Tcl_Pid) -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindExecutable --
 *
 *	This procedure computes the absolute path name of the current
 *	application, given its argv[0] value.  However, this
 *	implementation doesn't use of need the argv[0] value.  NULL
 *	may be passed in its place.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The variable tclExecutableName gets filled in with the file
 *	name for the application, if we figured it out.  If we couldn't
 *	figure it out, Tcl_FindExecutable is set to NULL.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FindExecutable(
    char *argv0)		/* The value of the application's argv[0]. */
{
    ProcessSerialNumber psn;
    ProcessInfoRec info;
    Str63 appName;
    FSSpec fileSpec;
    int pathLength;
    Handle pathName = NULL;
    OSErr err;
    
    GetCurrentProcess(&psn);
    info.processInfoLength = sizeof(ProcessInfoRec);
    info.processName = appName;
    info.processAppSpec = &fileSpec;
    GetProcessInformation(&psn, &info);

    if (tclExecutableName != NULL) {
	ckfree(tclExecutableName);
	tclExecutableName = NULL;
    }
    
    err = FSpPathFromLocation(&fileSpec, &pathLength, &pathName);

    tclExecutableName = (char *) ckalloc((unsigned) pathLength + 1);
    HLock(pathName);
    strcpy(tclExecutableName, *pathName);
    HUnlock(pathName);
    DisposeHandle(pathName);	
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetUserHome --
 *
 *	This function takes the passed in user name and finds the
 *	corresponding home directory specified in the password file.
 *
 * Results:
 *	On a Macintosh we always return a NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetUserHome(
    char *name,			/* User name to use to find home directory. */
    Tcl_DString *bufferPtr)	/* May be used to hold result.  Must not hold
				 * anything at the time of the call, and need
				 * not even be initialized. */
{
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMatchFiles --
 *
 *	This routine is used by the globbing code to search a
 *	directory for all files which match a given pattern.
 *
 * Results: 
 *	If the tail argument is NULL, then the matching files are
 *	added to the interp->result.  Otherwise, TclDoGlob is called
 *	recursively for each matching subdirectory.  The return value
 *	is a standard Tcl result indicating whether an error occurred
 *	in globbing.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- */

int
TclMatchFiles(
    Tcl_Interp *interp,		/* Interpreter to receive results. */
    char *separators,		/* Directory separators to pass to TclDoGlob. */
    Tcl_DString *dirPtr,	/* Contains path to directory to search. */
    char *pattern,		/* Pattern to match against. */
    char *tail)			/* Pointer to end of pattern.  Tail must
				 * point to a location in pattern. */
{
    char *dirName, *patternEnd = tail;
    char savedChar;
    int result = TCL_OK;
    int baseLength = Tcl_DStringLength(dirPtr);
    CInfoPBRec pb;
    OSErr err;
    FSSpec dirSpec;
    Boolean isDirectory;
    long dirID;
    short itemIndex;
    Str255 fileName;
    

    /*
     * Make sure that the directory part of the name really is a
     * directory.
     */

    dirName = dirPtr->string;
    FSpLocationFromPath(strlen(dirName), dirName, &dirSpec);
    err = FSpGetDirectoryID(&dirSpec, &dirID, &isDirectory);
    if ((err != noErr) || !isDirectory) {
	return TCL_OK;
    }

    /*
     * Now open the directory for reading and iterate over the contents.
     */

    pb.hFileInfo.ioVRefNum = dirSpec.vRefNum;
    pb.hFileInfo.ioDirID = dirID;
    pb.hFileInfo.ioNamePtr = (StringPtr) fileName;
    pb.hFileInfo.ioFDirIndex = itemIndex = 1;
    
    /*
     * Clean up the end of the pattern and the tail pointer.  Leave
     * the tail pointing to the first character after the path separator
     * following the pattern, or NULL.  Also, ensure that the pattern
     * is null-terminated.
     */

    if (*tail == '\\') {
	tail++;
    }
    if (*tail == '\0') {
	tail = NULL;
    } else {
	tail++;
    }
    savedChar = *patternEnd;
    *patternEnd = '\0';

    while (1) {
	pb.hFileInfo.ioFDirIndex = itemIndex;
	pb.hFileInfo.ioDirID = dirID;
	err = PBGetCatInfoSync(&pb);
	if (err != noErr) {
	    break;
	}

	/*
	 * Now check to see if the file matches.  If there are more
	 * characters to be processed, then ensure matching files are
	 * directories before calling TclDoGlob. Otherwise, just add
	 * the file to the result.
	 */

	p2cstr(fileName);
	if (Tcl_StringMatch((char *) fileName, pattern)) {
	    Tcl_DStringSetLength(dirPtr, baseLength);
	    Tcl_DStringAppend(dirPtr, (char *) fileName, -1);
	    if (tail == NULL) {
		if ((dirPtr->length > 1) &&
			(strchr(dirPtr->string+1, ':') == NULL)) {
		    Tcl_AppendElement(interp, dirPtr->string+1);
		} else {
		    Tcl_AppendElement(interp, dirPtr->string);
		}
	    } else if ((pb.hFileInfo.ioFlAttrib & ioDirMask) != 0) {
		Tcl_DStringAppend(dirPtr, ":", 1);
		result = TclDoGlob(interp, separators, dirPtr, tail);
		if (result != TCL_OK) {
		    break;
		}
	    }
	}
	
	itemIndex++;
    }
    *patternEnd = savedChar;

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacStat --
 *
 *	This function replaces the library version of stat.  The stat
 *	function provided by most Mac compiliers is rather broken and
 *	incomplete.
 *
 * Results:
 *	See stat documentation.
 *
 * Side effects:
 *	See stat documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclMacStat(
    char *path,
    struct stat *buf)
{
    HFileInfo fpb;
    HVolumeParam vpb;
    OSErr err;
    FSSpec fileSpec;
    Boolean isDirectory;
    long dirID;
    
    err = FSpLocationFromPath(strlen(path), path, &fileSpec);
    if (err != noErr) {
	errno = TclMacOSErrorToPosixError(err);
	return -1;
    }
    
    /*
     * Fill the fpb & vpb struct up with info about file or directory.
     */
     
    FSpGetDirectoryID(&fileSpec, &dirID, &isDirectory);
    vpb.ioVRefNum = fpb.ioVRefNum = fileSpec.vRefNum;
    vpb.ioNamePtr = fpb.ioNamePtr = fileSpec.name;
    if (isDirectory) {
	fpb.ioDirID = fileSpec.parID;
    } else {
	fpb.ioDirID = dirID;
    }

    fpb.ioFDirIndex = 0;
    err = PBGetCatInfoSync((CInfoPBPtr)&fpb);
    if (err == noErr) {
	vpb.ioVolIndex = 0;
	err = PBHGetVInfoSync((HParmBlkPtr)&vpb);
	if (err == noErr && buf != NULL) {
	    /* 
	     * Files are always readable by everyone.
	     */
	     
	    buf->st_mode = S_IRUSR | S_IRGRP | S_IROTH;

	    /* 
	     * Use the Volume Info & File Info to fill out stat buf.
	     */
	    if (fpb.ioFlAttrib & 0x10) {
		buf->st_mode |= S_IFDIR;
		buf->st_nlink = 2;
	    } else {
		buf->st_nlink = 1;
		if (fpb.ioFlFndrInfo.fdFlags & 0x8000) {
		    buf->st_mode |= S_IFLNK;
		} else {
		    buf->st_mode |= S_IFREG;
		}
	    }
	    if ((fpb.ioFlAttrib & 0x10) || (fpb.ioFlFndrInfo.fdType == 'APPL')) {
	    	/*
	    	 * Directories and applications are executable by everyone.
	    	 */
	    	 
	    	buf->st_mode |= S_IXUSR | S_IXGRP | S_IXOTH;
	    }
	    if ((fpb.ioFlAttrib & 0x01) == 0){
	    	/* 
	    	 * If not locked, then everyone has write acces.
	    	 */
	    	 
	        buf->st_mode |= S_IWUSR | S_IWGRP | S_IWOTH;
	    }
	    buf->st_ino = fpb.ioDirID;
	    buf->st_dev = fpb.ioVRefNum;
	    buf->st_uid = -1;
	    buf->st_gid = -1;
	    buf->st_rdev = 0;
	    buf->st_size = fpb.ioFlLgLen;
	    buf->st_blksize = vpb.ioVAlBlkSiz;
	    buf->st_blocks = (buf->st_size + buf->st_blksize - 1)
		/ buf->st_blksize;

	    /*
	     * The times returned by the Mac file system are in the
	     * local time zone.  We convert them to GMT so that the
	     * epoch starts from GMT.  This is also consistant with
	     * what is returned from "clock seconds".
	     */
	    if (initalized == false) {
		MachineLocation loc;
    
		ReadLocation(&loc);
		gmt_offset = loc.u.gmtDelta & 0x00ffffff;
		if (gmt_offset & 0x00800000) {
		    gmt_offset = gmt_offset | 0xff000000;
		}
		initalized = true;
	    }
	    buf->st_atime = buf->st_mtime = fpb.ioFlMdDat - gmt_offset;
	    buf->st_ctime = fpb.ioFlCrDat - gmt_offset;

	}
    }

    if (err != noErr) {
	errno = TclMacOSErrorToPosixError(err);
    }
    
    return (err == noErr ? 0 : -1);
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacReadlink --
 *
 *	This function replaces the library version of readlink.
 *
 * Results:
 *	See readlink documentation.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclMacReadlink(
    char *path,
    char *buf,
    int size)
{
    HFileInfo fpb;
    OSErr err;
    FSSpec fileSpec;
    Boolean isDirectory;
    Boolean wasAlias;
    long dirID;
    char fileName[256];
    char *end;
    Handle theString = NULL;
    int pathSize;

    /*
     * Remove ending colons if they exist.
     */
    while ((strlen(path) != 0) && (path[strlen(path) - 1] == ':')) {
	path[strlen(path) - 1] = NULL;
    }

    if (strchr(path, ':') == NULL) {
	strcpy(fileName, path);
	path = NULL;
    } else {
	end = strrchr(path, ':') + 1;
	strcpy(fileName, end);
	*end = NULL;
    }
    c2pstr(fileName);
    
    /*
     * Create the file spec for the directory of the file
     * we want to look at.
     */
    if (path != NULL) {
	err = FSpLocationFromPath(strlen(path), path, &fileSpec);
	if (err != noErr) {
	    errno = EINVAL;
	    return -1;
	}
    } else {
	FSMakeFSSpecCompat(0, 0, NULL, &fileSpec);
    }
    
    /*
     * Fill the fpb struct up with info about file or directory.
     */
    FSpGetDirectoryID(&fileSpec, &dirID, &isDirectory);
    fpb.ioVRefNum = fileSpec.vRefNum;
    fpb.ioDirID = dirID;
    fpb.ioNamePtr = (StringPtr) fileName;

    fpb.ioFDirIndex = 0;
    err = PBGetCatInfoSync((CInfoPBPtr)&fpb);
    if (err != noErr) {
	errno = TclMacOSErrorToPosixError(err);
	return -1;
    } else {
	if (fpb.ioFlAttrib & 0x10) {
	    errno = EINVAL;
	    return -1;
	} else {
	    if (fpb.ioFlFndrInfo.fdFlags & 0x8000) {
		/*
		 * The file is a link!
		 */
	    } else {
		errno = EINVAL;
		return -1;
	    }
	}
    }
    
    /*
     * If we are here it's really a link - now find out
     * where it points to.
     */
    err = FSMakeFSSpecCompat(fileSpec.vRefNum, dirID, (StringPtr) fileName, &fileSpec);
    if (err == noErr) {
	err = ResolveAliasFile(&fileSpec, true, &isDirectory, &wasAlias);
    }
    if ((err == fnfErr) || wasAlias) {
	err = FSpPathFromLocation(&fileSpec, &pathSize, &theString);
	if ((err != noErr) || (pathSize > size)) {
	    DisposeHandle(theString);
	    errno = ENAMETOOLONG;
	    return -1;
	}
    } else {
    	errno = EINVAL;
	return -1;
    }

    strncpy(buf, *theString, pathSize);
    DisposeHandle(theString);
    
    return pathSize;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacAccess --
 *
 *	This function replaces the library version of access.  The
 *	access function provided by most Mac compiliers is rather 
 *	broken or incomplete.
 *
 * Results:
 *	See access documentation.
 *
 * Side effects:
 *	See access documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclMacAccess(
    const char *path,
    int mode)
{
    HFileInfo fpb;
    HVolumeParam vpb;
    OSErr err;
    FSSpec fileSpec;
    Boolean isDirectory;
    long dirID;
    int full_mode = 0;

    err = FSpLocationFromPath(strlen(path), (char *) path, &fileSpec);
    if (err != noErr) {
	errno = TclMacOSErrorToPosixError(err);
	return -1;
    }
    
    /*
     * Fill the fpb & vpb struct up with info about file or directory.
     */
    FSpGetDirectoryID(&fileSpec, &dirID, &isDirectory);
    vpb.ioVRefNum = fpb.ioVRefNum = fileSpec.vRefNum;
    vpb.ioNamePtr = fpb.ioNamePtr = fileSpec.name;
    if (isDirectory) {
	fpb.ioDirID = fileSpec.parID;
    } else {
	fpb.ioDirID = dirID;
    }

    fpb.ioFDirIndex = 0;
    err = PBGetCatInfoSync((CInfoPBPtr)&fpb);
    if (err == noErr) {
	vpb.ioVolIndex = 0;
	err = PBHGetVInfoSync((HParmBlkPtr)&vpb);
	if (err == noErr) {
	    /* 
	     * Use the Volume Info & File Info to determine
	     * access information.  If we have got this far
	     * we know the directory is searchable or the file
	     * exists.  (We have F_OK)
	     */

	    /*
	     * Check to see if the volume is hardware or
	     * software locked.  If so we arn't W_OK.
	     */
	    if (mode & W_OK) {
		if ((vpb.ioVAtrb & 0x0080) || (vpb.ioVAtrb & 0x8000)) {
		    errno = EROFS;
		    return -1;
		}
		if (fpb.ioFlAttrib & 0x01) {
		    errno = EACCES;
		    return -1;
		}
	    }
	    
	    /*
	     * Directories are always searchable and executable.  But only 
	     * files of type 'APPL' are executable.
	     */
	    if (!(fpb.ioFlAttrib & 0x10) && (mode & X_OK)
	    	&& (fpb.ioFlFndrInfo.fdType != 'APPL')) {
		return -1;
	    }
	}
    }

    if (err != noErr) {
	errno = TclMacOSErrorToPosixError(err);
	return -1;
    }
    
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacFOpenHack --
 *
 *	This function replaces fopen.  It supports paths with alises.
 *	Note, remember to undefine the fopen macro!
 *
 * Results:
 *	See fopen documentation.
 *
 * Side effects:
 *	See fopen documentation.
 *
 *----------------------------------------------------------------------
 */

#undef fopen
FILE *
TclMacFOpenHack(
    const char *path,
    const char *mode)
{
    OSErr err;
    FSSpec fileSpec;
    Handle pathString = NULL;
    int size;
    FILE * f;
    
    err = FSpLocationFromPath(strlen(path), (char *) path, &fileSpec);
    if ((err != noErr) && (err != fnfErr)) {
	return NULL;
    }
    err = FSpPathFromLocation(&fileSpec, &size, &pathString);
    if ((err != noErr) && (err != fnfErr)) {
	return NULL;
    }
    
    HLock(pathString);
    f = fopen(*pathString, mode);
    HUnlock(pathString);
    DisposeHandle(pathString);
    return f;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacOSErrorToPosixError --
 *
 *	Given a Macintosh OSErr return the appropiate POSIX error.
 *
 * Results:
 *	A Posix error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclMacOSErrorToPosixError(
    int error)	/* A Macintosh error. */
{
    switch (error) {
	case noErr:
	    return 0;
	case bdNamErr:
	    return ENAMETOOLONG;
	case afpObjectTypeErr:
	    return ENOTDIR;
	case fnfErr:
	case dirNFErr:
	    return ENOENT;
	case dupFNErr:
	    return EEXIST;
	case dirFulErr:
	case dskFulErr:
	    return ENOSPC;
	case fBsyErr:
	    return EBUSY;
	case tmfoErr:
	    return ENFILE;
	case fLckdErr:
	case permErr:
	case afpAccessDenied:
	    return EACCES;
	case wPrErr:
	case vLckdErr:
	    return EROFS;
	case badMovErr:
	    return EINVAL;
	case diffVolErr:
	    return EXDEV;
	default:
	    return EINVAL;
    }
}
