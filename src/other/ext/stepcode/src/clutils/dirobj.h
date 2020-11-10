
#ifndef dirobj_h
#define dirobj_h

/*
* NIST Utils Class Library
* clutils/dirobj.h
* April 1997
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/*
 * DirObj - This object contains a list of files in a directory.
 *      Can be used for checking paths etc.
 *  based on InterViews FBDirectory
 *      Copyright (c) 1987, 1988, 1989 Stanford University from
 *      /depot/iv/src/libInterViews/filebrowser.c <= notice
 *  I commented it, changed variable and function names, added a
 *  few things, and made it so we can actually use it.
 *      David Sauder
 */

///////////////////////////////////////////////////////////////////////////////
//
// These header files are a pain between compilers etc ... and
// I agree they are in a mess.  You will have to CHECK them yourself
// depending on what you are using -- it works the way it is with g++
// but you need -I/depot/gnu/lib/g++-include
//
///////////////////////////////////////////////////////////////////////////////

#include <sc_cf.h>
#include <sc_export.h>
#include <stdlib.h>

#include <string.h>
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#include <string>


/*****************************************************************************/

class SC_UTILS_EXPORT DirObj {
    public:
        DirObj( const char * dirName );
        virtual ~DirObj();

        bool LoadDirectory( const std::string & name );
        static std::string Normalize( const std::string & s );

        const char * ValidDirectories( const char * );

        int Index( const char * );
        const char * File( int index );
        // check for file in the currently loaded directory
        bool FileExists( const char * file ) {
            return Index( file ) ? 1 : 0;
        }
        bool FileExists( const std::string & file ) {
            return Index( file.c_str() ) ? true : false;
        }
        int Count();

        static bool IsADirectory( const char * );
    private:
        const char * RealPath( const char * );

        bool Reset( const std::string & path );
        void ClearFileList();
        void CheckIndex( int index );
        void InsertFile( const char *, int index );
        void AppendFile( const char * );
        void RemoveFile( int index );
        virtual int Position( const char * );
    private:
        char ** fileList;
        int fileCount;
        int fileListSize;
};

// Return the number of files in the loaded directory.
inline int DirObj::Count() {
    return fileCount;
}

// Insert a new file into the fileList.
inline void DirObj::AppendFile( const char * s ) {
    InsertFile( s, fileCount );
}

// Return the file at the given index (starting at 0) in the fileList
inline const char * DirObj::File( int index ) {
    return ( 0 <= index && index < fileCount ) ? fileList[index] : 0;
}

#endif

