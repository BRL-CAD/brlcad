
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

#include <sc_cf.h>
#include <dirobj.h>
#ifdef HAVE_DIRENT_H
# include <dirent.h>
#endif

/* for stat() file status */
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#include <string>
#include <iostream>

#if defined(__WIN32__)
#include <shlwapi.h>
#endif

#include <sc_memmgr.h>

///////////////////////////////////////////////////////////////////////////////
//
// Create a new DirObj object.
//
///////////////////////////////////////////////////////////////////////////////

DirObj::DirObj( const char * dirName ) {
    const int defaultSize = 256;

    fileListSize = defaultSize;
    fileList = new char*[fileListSize];
    fileCount = 0;
    LoadDirectory( dirName );
}

///////////////////////////////////////////////////////////////////////////////
//
// DirObj object destructor.
//
///////////////////////////////////////////////////////////////////////////////

DirObj::~DirObj() {
    ClearFileList();
    delete [] fileList;
}

//////////////////////////////// RealPath() ///////////////////////////////////
//
// Returns a real path.
// if 'path' is a null string ("\0") or nil (0) return ./
// otherwise return the path.
//
///////////////////////////////////////////////////////////////////////////////

const char * DirObj::RealPath( const char * path ) {
    const char * realpath;

    if( path == 0 || *path == '\0' ) {
        realpath = "./";
    } else {
        realpath = path;
    }
    return realpath;
}

//////////////////////////////// LoadDirectory() //////////////////////////////
//
// Load the directory at the end of the longest path given by 'name'
//
///////////////////////////////////////////////////////////////////////////////

bool DirObj::LoadDirectory( const std::string & name ) {
    if( name.empty() ) {
        return Reset( "./" );
    } else {
        return Reset( name );
    }
}

//////////////////////////////// Index() //////////////////////////////////////
//
// Returns the index position of 'name' in the fileList; otherwise returns -1
// index starts at zero.
//
///////////////////////////////////////////////////////////////////////////////

int DirObj::Index( const char * name ) {
    for( int i = 0; i < fileCount; ++i ) {
        if( strcmp( fileList[i], name ) == 0 ) {
            return i;
        }
    }
    return -1;
}

//////////////////////////////// Reset() //////////////////////////////////////
//
// Clears existing fileList and fills it again with the names of entries in
// the directory at the supplied 'path'.
// for type 'DIR' and 'direct' see dirent.h
//
///////////////////////////////////////////////////////////////////////////////

bool DirObj::Reset( const std::string & path ) {
    bool successful = IsADirectory( path.c_str() );
    if( successful ) {
#ifdef __WIN32__
        WIN32_FIND_DATA FindFileData;
        HANDLE hFind;

        ClearFileList();
        hFind = FindFirstFile( path.c_str(), &FindFileData );
        if( hFind != INVALID_HANDLE_VALUE ) {
            int i = 0;
            do {
                InsertFile( FindFileData.cFileName, i++ );
            } while( FindNextFile( hFind, &FindFileData ) );
            FindClose( hFind );
        }
#else
        DIR * dir = opendir( path.c_str() );
        ClearFileList();

        for( struct dirent * d = readdir( dir ); d != NULL; d = readdir( dir ) ) {
            InsertFile( d->d_name, Position( d->d_name ) );
        }
        closedir( dir );
#endif
    } else {
        std::cout << "not a directory: " << path << "!" << std::endl;
    }
    return successful;
}

//////////////////////////////// IsADirectory() ///////////////////////////////
//
// Return 1 if file named by 'path' is a directory; otherwise 0.
// 'path' must not have tildes -- can send path to Normalize() first
// See stat.h
//
///////////////////////////////////////////////////////////////////////////////

bool DirObj::IsADirectory( const char * path ) {
#if defined(__WIN32__)
    if( PathIsDirectory( path ) ) {
        return true;
    }
    return false;
#else
    struct stat st;
    return stat( path, &st ) == 0 && ( st.st_mode & S_IFMT ) == S_IFDIR;
#endif
}

//////////////////////////////// Normalize() //////////////////////////////////
//
// Normalize path in order in the following way:
//  set path to:
//  1) remaining path starting at the last '/' in multiple '/'s in a row
//  2) path with all './'s removed from path except a possible leading './'
//  3) path with all '../'s collapsed from path except a possible leading '../'
//  4)  if path[0] is now '\0' set path to "./" or
//  if path doesn't start with './' or '../' or '/'
//      make it start with './' or
//  if path doesn't end with '/'
//      make it end with '/'
//  Soooooo, should end up with a path:
//    1) starting with './', '..', or '/'
//    2) with all internal './'and '../' removed and collapsed respectively
//    3) ending with '/'
//
//    disabled - 3) the last ~ or ~name expanded fully
//
///////////////////////////////////////////////////////////////////////////////

std::string DirObj::Normalize( const std::string & path ) {
    std::string buf;
    const char * slash;
#if defined(__WIN32__)
    char b[MAX_PATH];
    PathCanonicalize( b, path.c_str() );
    slash = "\\";
#else
    char * b;
    b = realpath( path.c_str(), 0 );
    slash = "/";
#endif
    if( b == 0 ) {
        buf.clear();
    } else {
        buf.assign( b );

#if !defined(__WIN32__)
	free(b);
#endif
    }

    if( buf.empty() ) {
        buf = ".";
        buf.append( slash );

        // if buf is a path to a directory and doesn't end with '/'
    } else if( IsADirectory( buf.c_str() ) && buf[buf.size()] != slash[0] ) {
        buf.append( slash );
    }
    return buf;
}

//////////////////////////////// ValidDirectories() ///////////////////////////
//
// Return the longest valid path possible from the given 'path'
//
///////////////////////////////////////////////////////////////////////////////

const char * DirObj::ValidDirectories( const char * path ) {
#ifdef __WIN32__
    static char buf[MAX_PATH + 1];
#else
    static char buf[MAXPATHLEN + 1];
#endif
    strcpy( buf, path );
    int i = strlen( path );

    while( !IsADirectory( RealPath( buf ) ) && i >= 0 ) {
        for( --i; buf[i] != '/' && i >= 0; --i ) {
            ;
        }
        buf[i + 1] = '\0';
    }
    return buf;
}

//////////////////////////////// CheckIndex() /////////////////////////////////
//
// Check to see if index is within fileListSize.  If not, make the number of
// pointers in fileList and fileListSize double that of index+1.
//
///////////////////////////////////////////////////////////////////////////////

void DirObj::CheckIndex( int index ) {
    char ** newstrbuf;

    if( index >= fileListSize ) {
        fileListSize = ( index + 1 ) * 2;
        newstrbuf = new char*[fileListSize];
        memmove( newstrbuf, fileList, fileCount * sizeof( char * ) );
        delete [] fileList;
        fileList = newstrbuf;
    }
}

//////////////////////////////// InsertFile() /////////////////////////////////
//
// Insert a file into the fileList.
//
///////////////////////////////////////////////////////////////////////////////

void DirObj::InsertFile( const char * f, int index ) {
    char ** spot;
    index = ( index < 0 ) ? fileCount : index;

    if( index < fileCount ) {
        CheckIndex( fileCount + 1 );
        spot = &fileList[index];
        memmove( spot + 1, spot, ( fileCount - index )*sizeof( char * ) );
    } else {
        CheckIndex( index );
        spot = &fileList[index];
    }
#ifdef __MSVC__
    char * string = _strdup( f );
#else
    char * string = strdup( f );
#endif
    *spot = string;
    ++fileCount;
}

//////////////////////////////// RemoveFile() /////////////////////////////////
//
// Delete a file from the fileList
//
///////////////////////////////////////////////////////////////////////////////

void DirObj::RemoveFile( int index ) {
    if( index < --fileCount ) {
        const char ** spot = ( const char ** )&fileList[index];
        delete spot;
        memmove( spot, spot + 1, ( fileCount - index )*sizeof( char * ) );
    }
}

//////////////////////////////// ClearFileList() //////////////////////////////
//
// Delete memory areas pointed to by fileList.
//
///////////////////////////////////////////////////////////////////////////////

void DirObj::ClearFileList() {
    for( int i = 0; i < fileCount; ++i ) {
        free( fileList[i] );
    }
    fileCount = 0;
}

//////////////////////////////// Position() ///////////////////////////////////
//
// Find the index where the file would go into an alphabetically sorted
// fileList.
//
///////////////////////////////////////////////////////////////////////////////

int DirObj::Position( const char * f ) {
    int i;

    for( i = 0; i < fileCount; ++i ) {
        if( strcmp( f, fileList[i] ) < 0 ) {
            return i;
        }
    }
    return fileCount;
}

