
/*
* NIST Utils Class Library
* clutils/dirobj.cc
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: dirobj.cc,v 3.0.1.3 1997/11/05 22:33:45 sauderd DP3.1 $  */ 

/*
 * DirObj implementation
 * based on FBDirectory from /depot/iv/src/libInterViews/filebrowser.c
 * David Sauder
 */
/*
 * Copyright (c) 1987, 1988, 1989 Stanford University
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Stanford not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Stanford makes no representations about
 * the suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * STANFORD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
 * IN NO EVENT SHALL STANFORD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <dirobj.h>
#include <dirent.h>

#ifdef HAVE_CONFIG_H
# include <scl_cf.h>
#endif

/* for getuid() */
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
/* for stat() file status */
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

// to help ObjectCenter
#ifndef HAVE_MEMMOVE
extern "C"
{
void * memmove(void *__s1, const void *__s2, size_t __n);
}
#endif

//#ifdef __OBJECTCENTER__ 
//extern "C"
//{
//void * memmove(void *__s1, const void *__s2, size_t __n);
//}
//#endif

///////////////////////////////////////////////////////////////////////////////
//
// Create a new DirObj object.
//
///////////////////////////////////////////////////////////////////////////////

DirObj::DirObj (const char* dirName) {
    const int defaultSize = 256;

    fileListSize = defaultSize;
    fileList = new char*[fileListSize];
    fileCount = 0;
    LoadDirectory(dirName);
}

///////////////////////////////////////////////////////////////////////////////
//
// DirObj object destructor.
//
///////////////////////////////////////////////////////////////////////////////

DirObj::~DirObj () {
    ClearFileList();
}

//////////////////////////////// RealPath() ///////////////////////////////////
//
// Returns a real path.  
// if 'path' is a null string ("\0") or nil (0) return ./
// otherwise return the path remaining after the following is done:
// send the remaining path after the first '/' in the last occurence
//	of '//' to InterpTilde() where the last tilde will be expanded
//	to a full path.
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::RealPath (const char* path) {
    const char* realpath;

    if (path == 0 || *path == '\0') {
        realpath = "./";
    } else {
        realpath = InterpTilde(InterpSlashSlash(path));
    }
    return realpath;
}

//////////////////////////////// LoadDirectory() //////////////////////////////
//
// Load the directory at the end of the longest path given by 'name'
//
///////////////////////////////////////////////////////////////////////////////

boolean DirObj::LoadDirectory (const char* name) {
    char buf[MAXPATHLEN+2];
    const char* path = buf;

    strcpy(buf, ValidDirectories(RealPath(name)));
    return Reset(buf);
}

//////////////////////////////// Index() //////////////////////////////////////
//
// Returns the index position of 'name' in the fileList; otherwise returns -1
// index starts at zero.
//
///////////////////////////////////////////////////////////////////////////////

int DirObj::Index (const char* name) {
    for (int i = 0; i < fileCount; ++i) {
        if (strcmp(fileList[i], name) == 0) {
            return i;
        }
    }
    return -1;
}

//////////////////////////////// Reset() //////////////////////////////////////
//
// Clears existing fileList and fills it again with the names of entries in 
// the directory at the supplied 'path'.
// for type 'DIR' and 'direct' see /usr/include/sys/dirent.h
//
///////////////////////////////////////////////////////////////////////////////

boolean DirObj::Reset (const char* path) {
    boolean successful = IsADirectory(path);
    if (successful) {
	DIR* dir = opendir(path);
        ClearFileList();

        for (struct dirent* d = readdir(dir); d != NULL; d = readdir(dir)) {
//#if defined(SYSV)
//        for (struct dirent* d = readdir(dir); d != NULL; d = readdir(dir)) {
//#else
//        for (struct direct* d = readdir(dir); d != NULL; d = readdir(dir)) {
//#endif
            InsertFile(d->d_name, Position(d->d_name));
        }
        closedir(dir);
    }
    return successful; 
}

//////////////////////////////// IsADirectory() ///////////////////////////////
//
// Return 1 if file named by 'path' is a directory; otherwise 0.
// 'path' must not have tildes -- can send path to Normalize() first
// /usr/include/sys/stat.h
//
///////////////////////////////////////////////////////////////////////////////

boolean DirObj::IsADirectory (const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

//////////////////////////////// Home() ///////////////////////////////////////
//
// Return the home directory (complete path) for user with login name 'name'.
// If 'name' is nil, it returns the home directory of the logged-in user who
// ran the program.  Returns nil on error - (name not in password file).
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::Home (const char* name) {
    struct passwd* pw =
        (name == nil) ? getpwuid(getuid()) : getpwnam((char *)name);
    return (pw == nil) ? nil : pw->pw_dir;
}

//////////////////////////////// Normalize() //////////////////////////////////
// 
// Normalize path in order in the following way:
//	set path to:
//  1) remaining path starting at the last '/' in multiple '/'s in a row
//  2) path with all './'s removed from path except a possible leading './'
//  2) path with all '../'s collapsed from path except a possible leading '../'
//  3) path with the last ~ or ~name expanded fully followed by the path
//	remaining after the last ~ or ~name (if any).
//  4)  if path[0] is now '\0' set path to "./" or 
//	if path doesn't start with './' or '../' or '/'
//		make it start with './' or 
//	if path doesn't end with '/'
//		make it end with '/'
//  Soooooo, should end up with a path:
//    1) starting with './', '..', or '/'
//    2) with all internal './'and '../' removed and collapsed respectively 
//    3) the last ~ or ~name expanded fully
//    4) ending with '/'
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::Normalize (const char* path) {
    static char newpath[MAXPATHLEN+1];
    const char* buf;

    buf = InterpSlashSlash(path);
    buf = ElimDot(buf);
    buf = ElimDotDot(buf);
    buf = InterpTilde(buf);

    if (*buf == '\0') {
        strcpy(newpath, "./");

	// if buf doesn't start with '.' or '..' and buf[0] != '/'
    } else if (!DotSlash(buf) && !DotDotSlash(buf) && *buf != '/') {
        strcpy(newpath, "./");
        strcat(newpath, buf);

	// if buf is a path to a directory and doesn't end with '/'
    } else if (IsADirectory(buf) && buf[strlen(buf)-1] != '/') {
        strcpy(newpath, buf);
        strcat(newpath, "/");

    } else {
        strcpy(newpath, buf);
    }
    return newpath;
}

//////////////////////////////// ValidDirectories() ///////////////////////////
//
// Return the longest valid path possible from the given 'path'
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::ValidDirectories (const char* path) {
    static char buf[MAXPATHLEN+1];
    strcpy(buf, path);
    int i = strlen(path);

    while (!IsADirectory(RealPath(buf)) && i >= 0) {
        for (--i; buf[i] != '/' && i >= 0; --i);
        buf[i+1] = '\0';
    }
    return buf;
}

//////////////////////////////// InterpSlashSlash() ///////////////////////////
//
// Searches from the end of 'path' backwards for the first occurence of
// a double slash and returns the remaining path starting at the 
// second / of a double /
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::InterpSlashSlash (const char* path) {
    for (int i = strlen(path)-1; i > 0; --i) {
        if (path[i] == '/' && path[i-1] == '/') {
            return &path[i];
        }
    }
    return path;
}

//////////////////////////////// ElimDot() ////////////////////////////////////
//
// Removes all './' from path unless it's the first two char's in the path
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::ElimDot (const char* path) {
    static char newpath[MAXPATHLEN+1];
    const char* src;
    char* dest = newpath;

    for (src = path; src < &path[strlen(path)]; ++src) {
	    // if not (src[0] == '.' and src[1] == ('/' or '\0'))
        if (!DotSlash(src)) {
            *dest++ = *src;

	    // if path preceeds DotSlash skip '.' now and '/' at loop 
       } else if (*(dest-1) == '/') {
            ++src;

	    // if no path preceeds DotSlash copy '.' now and '/' next loop
        } else {
            *dest++ = *src;
        }            
    }
    *dest = '\0';
    return newpath;
}

//////////////////////////////// CollapsedDotDotSlash() ///////////////////////
//
// path = char * to path being collapsed
// start = char * to 1 char past the last char in 'path' 
//		(where the ../ would be)
// This should be called CollapsedDotDotSlashIfPossible()
// Return 1
//	if collapsed ../ and should have
//	if didn't collapse ../ and should not have (for path = /)
//	    (should maybe return an error but this object seems to try to
//	     correct the path in this case or find the longest correct path
//	     in other cases)
// Return 0
//	if didn't collapse ../ and should have
//
///////////////////////////////////////////////////////////////////////////////

//static boolean CollapsedDotDotSlash (const char* path, const char*& start) {
// Josh L, 5/2/95
static boolean CollapsedDotDotSlash (const char* path, const char* start) {
       // fail  if 'start' is at beginning of path (there is no path) or
       //	if no directory is before start (no '/' before '../')
    if (path == start || *(start-1) != '/') {
        return 0;

	// succeed if 1st char in path is '/' and start points to the 
	//  next char (didn't collapse path and shouldn't have
    } else if (path == start-1 && *path == '/') {
        return 1;

    } else if (path == start-2) {               // NB: won't handle '//' right
        start = path;
        return *start != '.';

    } else if (path < start-2 && !DotDotSlash(start-3)) {
        for (start -= 2; path <= start; --start) {
            if (*start == '/') {
                ++start;
                return 1;
            }
        }
        start = path;
        return 1;
    }
    return 0;
}

//////////////////////////////// ElimDotDot() /////////////////////////////////
//
// Removes all '../' from path unless it's the first three char's in the path
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::ElimDotDot (const char* path) {
    static char newpath[MAXPATHLEN+1];
    const char* src;
    char* dest = newpath;

    for (src = path; src < &path[strlen(path)]; ++src) {
	    // if found ../ and could collapse it, skip the '..' now and 
	    //    the '/' at loop
//        if (DotDotSlash(src) && CollapsedDotDotSlash(newpath, dest)) {
// Josh L, 5/2/95
        if (DotDotSlash(src) && 
	    CollapsedDotDotSlash(newpath, (const char *)dest)) {
            src += 2;
        } else { // copy char in path
            *dest++ = *src;
        }
    }
    *dest = '\0';
    return newpath;
}

//////////////////////////////// InterpTilde() ////////////////////////////////
//
// if 'path' has no tilde or an error happens when expanding the tilde,
//	return 'path'.
// if 'path' has a tilde, it must be at the beginning of 'path' or
//    follow a '/'
//	if a 'name' follows tilde, return a new path consisting of the
//	    last ~ in 'path' expanded to the 'name's home directory 
//	    followed by the remaining path after name (if such exists)
//	if no 'name' follows tilde, return a new path consisting of the
//	    last ~ in 'path' expanded to the home directory of the
//	    logged-in user who ran the process followed by the remaining 
//	    path after ~ (if such exists)
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::InterpTilde (const char* path) {
    static char realpath[MAXPATHLEN+1];
    const char* beg = strrchr(path, '~');
    boolean validTilde = beg != NULL && (beg == path || *(beg-1) == '/');

    if (validTilde) {
        const char* end = strchr(beg, '/');
        int length = (end == nil) ? strlen(beg) : end - beg;
        const char* expandedTilde = ExpandTilde(beg, length);

        if (expandedTilde == nil) {
            validTilde = 0;
        } else {
            strcpy(realpath, expandedTilde);
            if (end != nil) {
                strcat(realpath, end);
            }
        }
    }
    return validTilde ? realpath : path;
}

//////////////////////////////// ExpandTilde() ////////////////////////////////
//
// 'tildeName' must start with a tilde.
// if a name immediately follows ~ return a path expanded to the
//	name's home directory.
// if tildeName is only a ~ or if a '/' immediately follows ~ 
//	return a path expanded to the home directory of the logged-in 
//	user who ran the program.
// return nil on error (if the name is not in the password file)
//
///////////////////////////////////////////////////////////////////////////////

const char* DirObj::ExpandTilde (const char* tildeName, int length) {
    const char* name = nil;

    if (length > 1) {
        static char buf[MAXNAMLEN+1];
        strncpy(buf, tildeName+1, length-1);
        buf[length-1] = '\0';
        name = buf;
    }
    return Home(name);
}        

//////////////////////////////// CheckIndex() /////////////////////////////////
//
// Check to see if index is within fileListSize.  If not, make the number of
// pointers in fileList and fileListSize double that of index+1.
//
///////////////////////////////////////////////////////////////////////////////

void DirObj::CheckIndex (int index) {
    char** newstrbuf;

    if (index >= fileListSize) {
        fileListSize = (index+1) * 2;
        newstrbuf = new char*[fileListSize];
//        bcopy(fileList, newstrbuf, fileCount*sizeof(char*));
// Josh L, 5/2/95
//        memcpy(newstrbuf, fileList, fileCount*sizeof(char*));
// Dave memcpy is not working since memory areas overlap
        memmove(newstrbuf, fileList, fileCount*sizeof(char*));
        delete fileList;
        fileList = newstrbuf;
    }
}

//////////////////////////////// InsertFile() /////////////////////////////////
//
// Insert a file into the fileList.
//
///////////////////////////////////////////////////////////////////////////////

void DirObj::InsertFile (const char* f, int index) {
    char** spot;
    index = (index < 0) ? fileCount : index;

    if (index < fileCount) {
        CheckIndex(fileCount+1);
        spot = &fileList[index];
//        memcpy(spot+1, spot, (fileCount - index)*sizeof(char*));
// Dave memcpy is not working since memory areas overlap
        memmove(spot+1, spot, (fileCount - index)*sizeof(char*));
    } else {
        CheckIndex(index);
        spot = &fileList[index];
    }
#ifdef __O3DB__
    char* string = new char [strlen (f)];
    strcpy (string, f);
#else
    char* string = strdup(f);
#endif
    *spot = string;
    ++fileCount;
}

//////////////////////////////// RemoveFile() /////////////////////////////////
//
// Delete a file from the fileList
//
///////////////////////////////////////////////////////////////////////////////

void DirObj::RemoveFile (int index) {
    if (index < --fileCount) {
//        const char** spot = &fileList[index];
// Josh L, 5/2/95
        const char** spot = (const char**)&fileList[index];
        delete spot;
//        bcopy(spot+1, spot, (fileCount - index)*sizeof(char*));
// Josh L, 5/2/95
//        memcpy(spot, spot+1, (fileCount - index)*sizeof(char*));
// Dave memcpy is not working since memory areas overlap
        memmove(spot, spot+1, (fileCount - index)*sizeof(char*));
    }
}

//////////////////////////////// ClearFileList() //////////////////////////////
//
// Delete memory areas pointed to by fileList.
//
///////////////////////////////////////////////////////////////////////////////

void DirObj::ClearFileList () {
    for (int i = 0; i < fileCount; ++i) {
      free(fileList[i]);
    }
    fileCount = 0;
}

//////////////////////////////// Position() ///////////////////////////////////
//
// Find the index where the file would go into an alphabetically sorted
// fileList.
//
///////////////////////////////////////////////////////////////////////////////

int DirObj::Position (const char* f) {
    int i;

    for (i = 0; i < fileCount; ++i) {
        if (strcmp(f, fileList[i]) < 0) {
            return i;
        }
    }
    return fileCount;
}

