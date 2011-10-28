#ifndef BASIC_H
#define BASIC_H

/* $Id: basic.h,v 1.8 1997/10/22 16:06:28 sauderd Exp $ */

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: basic.h,v $
 * Revision 1.8  1997/10/22 16:06:28  sauderd
 * Added a help for the Centerline C compiler. It doesn't define __STDC__ but
 * it does understand prototypes (which is required to build the express toolkit)
 * The standard prototypes aren't turned on unless __STDC__ is turned on. I
 * changed it so that it is turned on for Centerline.
 *
 * Revision 1.7  1996/12/18 20:50:29  dar
 * updated for C++ compatibility
 *
 * Revision 1.6  1994/05/11  19:51:39  libes
 * numerous fixes
 *
 * Revision 1.5  1993/10/15  18:49:23  libes
 * CADDETC certified
 *
 * Revision 1.4  1993/03/22  18:07:15  libes
 * deleted MIN/MAX.  Not used, and wrong to boot.
 *
 * Revision 1.3  1993/01/19  22:45:07  libes
 * *** empty log message ***
 *
 * Revision 1.2  1992/08/18  17:15:40  libes
 * rm'd extraneous error messages
 *
 * Revision 1.1  1992/05/28  03:56:02  libes
 * Initial revision
 *
 * Revision 1.3  1992/02/12  07:06:15  libes
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:47:45  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:40:30  libes
 * Initial revision
 *
 * Revision 1.1  1992/01/22  02:17:49  libes
 * Initial revision
 *
 * Revision 1.5  1992/01/15  19:49:04  shepherd
 *  Commented out text after #else and #endif.
 *
 * Revision 1.2  91/01/14  13:34:14  silver
 * moeimodified to remove ANSI C compiler warning messages from the 
 * preprocessor directives.
 * 
 * Revision 1.1  91/01/09  15:25:16  laurila
 * Initial revision
 * 
 * Revision 1.3  90/09/25  10:01:36  clark
 * Beta checkin at SCRA
 * 
 * Revision 1.3  90/09/25  10:01:36  clark
 * Put wrapper around static_inline stuff,
 *     checking for previous definition.
 * 
 * Revision 1.2  90/09/04  15:05:51  clark
 * BPR 2.1 alpha
 * 
 * Revision 1.1  90/06/11  17:04:56  clark
 * Initial revision
 * 
 */

#include <stdio.h>

/******************************/
/* type Boolean and constants */
/******************************/

typedef enum Boolean_ { False, True} Boolean;

/************************/
/* Generic pointer type */
/************************/

#ifdef __STDC__
typedef void* Generic;
#else 
typedef char* Generic;
#endif    /*    */

/* other handy macros */
#define streq(x,y)	(!strcmp((x),(y)))


/**************************/
/* function pointer types */
/**************************/

typedef void (*voidFuncptr)();
typedef int (*intFuncptr)();

/******************************/
/* deal with inline functions */
/******************************/

#define static_inline static inline

/* allow same declarations to suffice for both Standard and Classic C */
/* ... at least in header files ... */

#ifndef CONST
# ifdef __STDC__
#  define CONST		const
# else
#  define CONST
# endif
#endif

#ifndef PROTO
# ifdef __STDC__
#  define PROTO(x)	x
# else

# ifdef __CLCC__
# define PROTO(x)	x
#else
# define PROTO(x)	()
# endif

# endif
#endif


/* pacify IBM's c89 */
#if !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

/* backward compatibility */
typedef char *String;
#define STRINGequal(x,y)	(0 == strcmp((x),(y)))


#endif    /*    */


