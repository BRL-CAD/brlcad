/****** RENAMED from xgetopt.cc to sc_getopt.cc ***********/

// XGetopt.cpp  Version 1.2
//
// Author:  Hans Dietrich
//          hdietrich2@hotmail.com
//
// Description:
//     XGetopt.cpp implements sc_getopt(), a function to parse command lines.
//
// History
//     Version 1.2 - 2003 May 17
//     - Added Unicode support
//
//     Version 1.1 - 2002 March 10
//     - Added example to XGetopt.cpp module header
//
// This software is released into the public domain.
// You are free to use it in any way you like.
//
// This software is provided "as is" with no expressed
// or implied warranty.  I accept no liability for any
// damage or loss of business that this software may cause.
//
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// if you are using precompiled headers then include this line:
//#include "stdafx.h"
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// if you are not using precompiled headers then include these lines:
//#include <windows.h>
#include <stdio.h>
#include <string.h>
///////////////////////////////////////////////////////////////////////////////

#include "sc_getopt.h"

///////////////////////////////////////////////////////////////////////////////
//
//  X G e t o p t . c p p
//
//
//  NAME
//       sc_getopt -- parse command line options
//
//  SYNOPSIS
//       int sc_getopt(int argc, char *argv[], char *optstring)
//
//       extern char *sc_optarg;
//       extern int sc_optind;
//
//  DESCRIPTION
//       The sc_getopt() function parses the command line arguments. Its
//       arguments argc and argv are the argument count and array as
//       passed into the application on program invocation.  In the case
//       of Visual C++ programs, argc and argv are available via the
//       variables __argc and __argv (double underscores), respectively.
//       sc_getopt returns the next option letter in argv that matches a
//       letter in optstring.  (Note:  Unicode programs should use
//       __targv instead of __argv.  Also, all character and string
//       literals should be enclosed in _T( ) ).
//
//       optstring is a string of recognized option letters;  if a letter
//       is followed by a colon, the option is expected to have an argument
//       that may or may not be separated from it by white space.  sc_optarg
//       is set to point to the start of the option argument on return from
//       sc_getopt.
//
//       Option letters may be combined, e.g., "-ab" is equivalent to
//       "-a -b".  Option letters are case sensitive.
//
//       sc_getopt places in the external variable sc_optind the argv index
//       of the next argument to be processed.  sc_optind is initialized
//       to 0 before the first call to sc_getopt.
//
//       When all options have been processed (i.e., up to the first
//       non-option argument), sc_getopt returns EOF, sc_optarg will point
//       to the argument, and sc_optind will be set to the argv index of
//       the argument.  If there are no non-option arguments, sc_optarg
//       will be set to NULL.
//
//       The special option "--" may be used to delimit the end of the
//       options;  EOF will be returned, and "--" (and everything after it)
//       will be skipped.
//
//  RETURN VALUE
//       For option letters contained in the string optstring, sc_getopt
//       will return the option letter.  sc_getopt returns a question mark (?)
//       when it encounters an option letter not included in optstring.
//       EOF is returned when processing is finished.
//
//  BUGS
//       1)  Long options are not supported.
//       2)  The GNU double-colon extension is not supported.
//       3)  The environment variable POSIXLY_CORRECT is not supported.
//       4)  The + syntax is not supported.
//       5)  The automatic permutation of arguments is not supported.
//       6)  This implementation of sc_getopt() returns EOF if an error is
//           encountered, instead of -1 as the latest standard requires.
//
//  EXAMPLE
//       BOOL CMyApp::ProcessCommandLine(int argc, char *argv[])
//       {
//           int c;
//
//           while ((c = sc_getopt(argc, argv, _T("aBn:"))) != EOF)
//           {
//               switch (c)
//               {
//                   case _T('a'):
//                       TRACE(_T("option a\n"));
//                       //
//                       // set some flag here
//                       //
//                       break;
//
//                   case _T('B'):
//                       TRACE( _T("option B\n"));
//                       //
//                       // set some other flag here
//                       //
//                       break;
//
//                   case _T('n'):
//                       TRACE(_T("option n: value=%d\n"), atoi(sc_optarg));
//                       //
//                       // do something with value here
//                       //
//                       break;
//
//                   case _T('?'):
//                       TRACE(_T("ERROR: illegal option %s\n"), argv[sc_optind-1]);
//                       return FALSE;
//                       break;
//
//                   default:
//                       TRACE(_T("WARNING: no handler for option %c\n"), c);
//                       return FALSE;
//                       break;
//               }
//           }
//           //
//           // check for non-option args here
//           //
//           return TRUE;
//       }
//
///////////////////////////////////////////////////////////////////////////////

char  * sc_optarg;        // global argument pointer
int     sc_optind = 0;     // global argv index

int sc_getopt( int argc, char * argv[], char * optstring ) {
    static char * next = NULL;
    if( sc_optind == 0 ) {
        next = NULL;
    }

    sc_optarg = NULL;

    if( next == NULL || *next == '\0' ) {
        if( sc_optind == 0 ) {
            sc_optind++;
        }

        if( sc_optind >= argc || argv[sc_optind][0] != '-' || argv[sc_optind][1] == '\0' ) {
            sc_optarg = NULL;
            if( sc_optind < argc ) {
                sc_optarg = argv[sc_optind];
            }
            return EOF;
        }

        if( strcmp( argv[sc_optind], "--" ) == 0 ) {
            sc_optind++;
            sc_optarg = NULL;
            if( sc_optind < argc ) {
                sc_optarg = argv[sc_optind];
            }
            return EOF;
        }

        next = argv[sc_optind];
        next++;     // skip past -
        sc_optind++;
    }

    char c = *next++;
    char * cp = strchr( optstring, c );

    if( cp == NULL || c == ':' ) {
        return '?';
    }

    cp++;
    if( *cp == ':' ) {
        if( *next != '\0' ) {
            sc_optarg = next;
            next = NULL;
        } else if( sc_optind < argc ) {
            sc_optarg = argv[sc_optind];
            sc_optind++;
        } else {
            return '?';
        }
    }

    return c;
}
