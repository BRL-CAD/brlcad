/**************************************************************************
                               NETPBM
                           pm_config.in.h
***************************************************************************
  This file provides platform-dependent definitions for all Netpbm
  libraries and the programs that use them.

  The make files generate pm_config.h by copying this file and adding
  other stuff.  The Netpbm programs #include pm_config.h.

  Wherever possible, Netpbm handles customization via the make files
  instead of via this file.  However, Netpbm's make file philosophy
  discourages lining up a bunch of -D options on every compile, so a 
  #define here would be preferable to a -D compile option.

**************************************************************************/

#if defined(USG) || defined(SVR4) || defined(__SVR4)
#define SYSV
#endif
#if !( defined(BSD) || defined(SYSV) || defined(MSDOS) || defined(__amigaos__))
/* CONFIGURE: If your system is >= 4.2BSD, set the BSD option; if you're a
** System V site, set the SYSV option; if you're IBM-compatible, set MSDOS;
** and if you run on an Amiga, set AMIGA. If your compiler is ANSI C, you're
** probably better off setting SYSV - all it affects is string handling.
*/
#define BSD
/* #define SYSV */
/* #define MSDOS */
#endif

/* Switch macros like _POSIX_SOURCE are supposed to add features from
   the indicated standard to the C library.  A source file defines one
   of these macros to declare that it uses features of that standard
   as opposed to conflicting features of other standards (e.g. the
   POSIX foo() subroutine might do something different from the X/Open
   foo() subroutine).  Plus, this forces the coder to understand upon
   what feature sets his program relies.

   But some C library developers have misunderstood this and think of these
   macros like the old __ansi__ macro, which tells the C library, "Don't 
   have any features that aren't in the ANSI standard."  I.e. it's just
   the opposite -- the macro subtracts features instead of adding them.

   This means that on some platforms, Netpbm programs must define
   _POSIX_SOURCE, and on others, it must not.  Netpbm's POSIX_IS_IMPLIED 
   macro indicates that we're on a platform where we need not define
   _POSIX_SOURCE (and probably must not).

   The problematic C libraries treat _XOPEN_SOURCE the same way.
*/
#if defined(__OpenBSD__) || defined (__NetBSD__) || defined(__bsdi__) || defined(__APPLE__)
#define POSIX_IS_IMPLIED
#endif


/* CONFIGURE: This is the name of an environment variable that tells
** where the color names database is.  If the environment variable isn't
** set, Netpbm tries the hardcoded defaults per macro 'RGB_DB_PATH'
** (see below).
*/
#define RGBENV "RGBDEF"    /* name of env-var */

/* CONFIGURE: There should be an environment variable telling where the color
** names database (color dictionary) is for Netpbm to use, e.g. to determine
** what colord name "Salmon" is.  The name of that environment variable is
** above.  But as some people prefer hardcoded file paths to environment
** variables, if such environment variable is not set, Netpbm looks for the
** first existing file in the list which is the value of 'RGB_DB_PATH'.  And
** if none of those exist (including if the list is empty), Netpbm simply
** doesn't understand any color names.  Note that Netpbm comes with a color
** database (lib/rgb.txt in the source tree), but you might choose to have
** Netpbm use a different one.  See the documentation of ppm_parsecolor()
** for the format of the color database file.
*/

#if (defined(SYSV) || defined(__amigaos__))

#include <string.h>

#ifndef __SASC
#ifndef _DCC    /* Amiga DICE Compiler */
#define bzero(dst,len) memset(dst,0,len)
#define bcopy(src,dst,len) memcpy(dst,src,len)
#define bcmp memcmp
#endif /* _DCC */
#endif /* __SASC */

#endif /*SYSV or Amiga*/

/* CONFIGURE: On most BSD systems, malloc() gets declared in stdlib.h, on
** system V, it gets declared in malloc.h. On some systems, malloc.h
** doesn't declare these, so we have to do it here. On other systems,
** for example HP/UX, it declares them incompatibly.  And some systems,
** for example Dynix, don't have a malloc.h at all.  A sad situation.
** If you have compilation problems that point here, feel free to tweak
** or remove these declarations.
*/
#ifdef BSD
#include <stdlib.h>
#endif
#if defined(SYSV)
#include <malloc.h>
#endif

/* MSVCRT means we're using the Microsoft Visual C++ runtime library.

   _WIN32, set by the compiler, apparently means the same thing; we see it set
   in compiles using the Microsoft Visual C++ development environment and also
   with Mingw, which is the Windows version of the GNU compiler (which brings
   with it a runtime library which wraps around the Microsoft one).  We don't
   see it set in Cygwin compiles, which use GNU libraries instead of the
   Microsoft one.

   There is also _MSC_VER, which is set by MSVC to the version number of the
   MSVC runtime library and __MINGW32__.
 */

#ifdef _WIN32
#define MSVCRT 1
#else
#define MSVCRT 0
#endif

/* WIN32 is a macro that some older compilers predefine (compilers aren't
   supposed to because it doesn't start with an underscore, hence the change.
   Many build systems (project files, etc.) set WIN32 explicitly for
   backward compatibility.  Netpbm doesn't use it.
*/

/* CONFIGURE: If your system has the setmode() function, set HAVE_SETMODE.
** If you do, and also the O_BINARY file mode, pm_init() will set the mode
** of stdin and stdout to binary for all Netpbm programs.
** You need this with Cygwin (Windows).
*/
#if MSVCRT || defined(__CYGWIN__) || defined(DJGPP)
#define HAVE_SETMODE
#endif

#if MSVCRT || defined(__CYGWIN__) || defined(DJGPP)
#define HAVE_IO_H 1
#else
#define HAVE_IO_H 0
#endif

#if (defined(__GLIBC__) || defined(__GNU_LIBRARY__) || defined(__APPLE__)) || defined(__NetBSD__)
  #define HAVE_VASPRINTF 1
#else
  #define HAVE_VASPRINTF 0
#endif

/* On Windows, unlinking a file is deleting it, and you can't delete an open
   file, so unlink of an open file fails.  The errno is (incorrectly) EACCES.
*/
#if MSVCRT || defined(__CYGWIN__) || defined(DJGPP)
  #define CAN_UNLINK_OPEN 0
#else
  #define CAN_UNLINK_OPEN 1
#endif

#ifdef __amigaos__
#include <clib/exec_protos.h>
#define getpid() ((pid_t)FindTask(NULL))
#endif

#ifdef DJGPP
#define lstat stat
#endif

/*  CONFIGURE: Netpbm uses __inline__ to declare functions that should
    be compiled as inline code.  GNU C recognizes the __inline__ keyword.
    If your compiler recognizes any other keyword for this, you can set
    it here.
*/
#if !defined(__GNUC__)
  #if (!defined(__inline__))
    #if (defined(__sgi) || defined(_AIX))
      #define __inline__ __inline
    #else   
      #define __inline__
    #endif
  #endif
#endif

/* At least one compiler can't handle two declarations of the same function
   that aren't literally identical.  E.g. "static foo_fn_t foo1;" conflicts
   with "static void foo1(int);" even if type 'foo_fn_t' is defined as
   void(int).  (The compiler we saw do this is SGI IDO cc (for IRIX 4.3)).

   LITERAL_FN_DEF_MATCH says that the compiler might have this problem,
   so one must be conservative in redeclaring functions.
*/
#if defined(__GNUC__)
  #define LITERAL_FN_DEF_MATCH 0
#else
  #if (defined(__sgi))
    #define LITERAL_FN_DEF_MATCH 1
  #else   
    #define LITERAL_FN_DEF_MATCH 0
  #endif
#endif

/* CONFIGURE: GNU Compiler extensions are used in performance critical places
   when available.  Test whether they exist.

   Prevent the build from exploiting these extensions by defining
   NO_GCC_UNIQUE.

   Before Netpbm 10.65 (December 2013), Netpbm used GCC compiler extensions
   to generate SSE code in Pamflip.  Starting in 10.65, Netpbm instead uses
   the more standard operators defined in <emmtrins.h>.  To prevent Netpbm
   from explicitly using any SSE instructions, set WANT_SSE to N in
   config.mk.
*/

#if defined(__GNUC__) && !defined(NO_GCC_UNIQUE)
  #define GCCVERSION __GNUC__*100 + __GNUC_MINOR__
#else
  #define GCCVERSION 0
#endif

#ifndef HAVE_GCC_BITCOUNT
#if GCCVERSION >=304
  #define HAVE_GCC_BITCOUNT 1
  /* Use __builtin_clz(),  __builtin_ctz() (and variants for long)
     to count leading/trailing 0s in int (and long). */
#else
  #define HAVE_GCC_BITCOUNT 0
#endif
#endif

#ifndef HAVE_GCC_BSWAP
#if GCCVERSION >=403 || defined(__clang__)
  #define HAVE_GCC_BSWAP 1
  /* Use __builtin_bswap32(), __builtin_bswap64() for endian conversion.
     NOTE: On intel CPUs this may produce the bswap operand which is not
     available on 80386. */
#else
  #define HAVE_GCC_BSWAP 0
#endif
#endif

#ifndef HAVE_WORKING_SSE2
#if defined(__SSE2__) && ( GCCVERSION >=402 || defined(__clang__) )
  #define HAVE_WORKING_SSE2 1
  /* We can use SSE2 builtin functions to exploit SSE2 instructions.  GCC
     version 4.2 or newer is required; older GCC ostensibly has these SSE2
     builtins, but the compiler aborts with an error.  Note that __SSE2__
     means not only that the compiler has the capability, but that the user
     has not disabled it via compiler options.
  */
#else
  #define HAVE_WORKING_SSE2 0
#endif
#endif

/* UNALIGNED_OK means it's OK to do unaligned memory access, e.g.
   loading an 8-byte word from an address that is not a multiple of 8.
   On some systems, such an access causes a trap and a signal.

   This determination is conservative - There may be cases where unaligned
   access is OK and we say here it isn't.

   We know unaligned access is _not_ OK on at least SPARC and some ARM.
*/

#if defined(__x86_64__) | defined(__i486__) | defined(__vax__)
# define UNALIGNED_OK 1
#else
# define UNALIGNED_OK 0
#endif


/* CONFIGURE: Some systems seem to need more than standard program linkage
   to get a data (as opposed to function) item out of a library.

   On Windows mingw systems, it seems you have to #include <import_mingw.h>
   and #define EXTERNDATA DLL_IMPORT  .  2001.05.19
*/
#define EXTERNDATA extern

/* only Pnmstitch uses UNREFERENCED_PARAMETER today (and I'm not sure why),
   but it might come in handy some day.
*/
#if (!defined(UNREFERENCED_PARAMETER))
# if (defined(__GNUC__))
#  define UNREFERENCED_PARAMETER(x)
# elif (defined(__USLC__) || defined(_M_XENIX))
#  define UNREFERENCED_PARAMETER(x) ((x)=(x))
# else
#  define UNREFERENCED_PARAMETER(x) (x)
# endif
#endif

#include <unistd.h>  /* Get _LFS_LARGEFILE defined */
#include <sys/types.h>
/* In GNU, _LFS_LARGEFILE means the "off_t" functions (ftello, etc.) are
   available.  In AIX, _AIXVERSION_430 means it's AIX Version 4.3.0 or
   better, which seems to mean the "off_t" functions are available.
*/
#if defined(_LFS_LARGEFILE) || defined(_AIXVERSION_430)
typedef off_t pm_filepos;
#define FTELLO ftello
#define FSEEKO fseeko
#else
typedef long int pm_filepos;
#define FTELLO ftell
#define FSEEKO fseek
#endif

#if defined(_PLAN9)
#define TMPDIR "/tmp"
#else
/* Use POSIX value P_tmpdir from libc */
#define TMPDIR P_tmpdir
#endif

/* Note that if you _don't_ have mkstemp(), you'd better have a safe
   mktemp() or otherwise not be concerned about its unsafety.  On some
   systems, use of mktemp() makes it possible for a hacker to cause a
   Netpbm program to access a file of the hacker's choosing when the
   Netpbm program means to access its own temporary file.
*/
#ifdef __MINGW32__
  #define HAVE_MKSTEMP 0
#else
  #define HAVE_MKSTEMP 1
#endif

typedef int qsort_comparison_fn(const void *, const void *);
    /* A compare function to pass to <stdlib.h>'s qsort() */

#if MSVCRT
  #define pm_mkdir(dir, perm) _mkdir(dir)
#else
  #define pm_mkdir(dir, perm) mkdir(dir, perm) 
#endif

#if MSVCRT
  #define pm_pipe _pipe
#else
  #define pm_pipe pipe
#endif
