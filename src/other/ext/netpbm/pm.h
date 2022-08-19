/* pm.h - interface to format-independent part of libpbm.
**
** Copyright (C) 1988, 1989, 1991 by Jef Poskanzer.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#ifndef PM_H_INCLUDED
#define PM_H_INCLUDED

#include "pm_config.h"

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

#if defined(_WIN32)
# define COMPILER_DLLEXPORT __declspec(dllexport)
# define COMPILER_DLLIMPORT __declspec(dllimport)
#else
# define COMPILER_DLLEXPORT __attribute__ ((visibility ("default")))
# define COMPILER_DLLIMPORT __attribute__ ((visibility ("default")))
#endif

#ifndef NETPBM_EXPORT
#  if defined(NETPBM_DLL_EXPORTS) && defined(NETPBM_DLL_IMPORTS)
#    error "Only NETPBM_DLL_EXPORTS or NETPBM_DLL_IMPORTS can be defined, not both."
#  elif defined(NETPBM_DLL_EXPORTS)
#    define NETPBM_EXPORT COMPILER_DLLEXPORT
#  elif defined(NETPBM_DLL_IMPORTS)
#    define NETPBM_EXPORT COMPILER_DLLIMPORT
#  else
#    define NETPBM_EXPORT
#  endif
#endif


/* Definitions to make Netpbm programs work with either ANSI C or C
   Classic.

   This is obsolete, as all compilers recognize the ANSI syntax now.

   We are slowly removing all the ARGS invocations from the programs
   (and replacing them with explicit ANSI syntax), but we have a lot
   of programs where we have removed ARGS from the definition but not
   the prototype, and we have discovered that the Sun compiler
   considers the resulting mismatch between definition and prototype
   to be an error.  So we make ARGS create the ANSI syntax
   unconditionally to avoid having to fix all those mismatches.  */

#if 0
#if __STDC__
#define ARGS(alist) alist
#else /*__STDC__*/
#define ARGS(alist) ()
#define const
#endif /*__STDC__*/
#endif
#define ARGS(alist) alist


/* PM_GNU_PRINTF_ATTR lets the GNU compiler check pm_message() and pm_error()
   calls to be sure the arguments match the format string, thus preventing
   runtime segmentation faults and incorrect messages.
*/
#ifdef __GNUC__
#define PM_GNU_PRINTF_ATTR(a,b) __attribute__ ((format (printf, a, b)))
#else
#define PM_GNU_PRINTF_ATTR(a,b)
#endif


/* PURE_FN_ATTR is the attribute you add to a function declaration
   that indicates it's a true function -- has no side effects and return
   value is not influenced by anything except its arguments.
*/
#ifdef __GNUC__
#define PURE_FN_ATTR __attribute__ ((const))
#else
#define PURE_FN_ATTR
#endif


/* S_IRUSR is POSIX, defined in <sys/stat.h> Some old BSD systems and Windows
   systems have S_IREAD instead.  Most Unix today (2011) has both.  In 2011,
   Android has S_IRUSR and not S_IREAD.

   Some Windows has _S_IREAD.

   We're ignoring S_IREAD now to see if anyone misses it.  If there are still
   users that need it, we can handle it here.
*/
#if MSVCRT
  #define PM_S_IWUSR _S_IWRITE
  #define PM_S_IRUSR _S_IREAD
#else
  #define PM_S_IWUSR S_IWUSR
  #define PM_S_IRUSR S_IRUSR
#endif



typedef struct {
    /* Coordinates of a pixel within an image.  Row 0 is the top row.
       Column 0 is the left column.
    */
    unsigned int row;
    unsigned int col;
} pm_pixelcoord;

extern int pm_plain_output;
    /* Output functions are to produce plain (as opposed to raw) format
       regardless of their 'plainformat' arguments.
    */
extern const char * pm_progname;

void 
pm_init(const char * const progname, unsigned int const flags);

void 
pm_proginit(int * const argcP, const char * argv[]);

void
pm_setMessage(int const newState, int * const oldStateP);

int
pm_getMessage(void);

FILE * 
pm_tmpfile(void);

int
pm_tmpfile_fd(void);

void
pm_make_tmpfile(FILE **       const filePP,
                const char ** const filenameP);

void
pm_make_tmpfile_fd(int *         const fdP,
                   const char ** const filenameP);

void
pm_nextimage(FILE * const file, int * const eofP);

/* Variable-sized arrays definitions. */

NETPBM_EXPORT extern char** 
pm_allocarray (int const cols, int const rows, int const size );

NETPBM_EXPORT extern char * 
pm_allocrow(unsigned int const cols,
            unsigned int const size);

NETPBM_EXPORT extern void 
pm_freearray (char** const its, int const rows);

NETPBM_EXPORT extern void 
pm_freerow(void * const row);


/* Obsolete -- use shhopt instead */
int 
pm_keymatch(const char * const str,
            const char * const keyword,
            int          const minchars);


int PURE_FN_ATTR
pm_maxvaltobits(int const maxval);

int PURE_FN_ATTR
pm_bitstomaxval(int const bits);

unsigned int PURE_FN_ATTR
pm_lcm (unsigned int const x, 
        unsigned int const y,
        unsigned int const z,
        unsigned int const limit);

void
pm_setjmpbuf(jmp_buf * const jmpbufP);

void
pm_setjmpbufsave(jmp_buf *  const jmpbufP,
                 jmp_buf ** const oldJmpbufPP);

void
pm_longjmp(void);

void
pm_fork(int *         const iAmParentP,
        pid_t *       const childPidP,
        const char ** const errorP);

void
pm_waitpid(pid_t         const pid,
           int *         const statusP,
           int           const options,
           pid_t *       const exitedPidP,
           const char ** const errorP);


void
pm_waitpidSimple(pid_t const pid);

typedef void pm_usermessagefn(const char * msg);

void
pm_setusermessagefn(pm_usermessagefn * fn);

typedef void pm_usererrormsgfn(const char * msg);

void
pm_setusererrormsgfn(pm_usererrormsgfn * fn);

void PM_GNU_PRINTF_ATTR(1,2)
pm_message (const char format[], ...);     

void PM_GNU_PRINTF_ATTR(1,2)
pm_errormsg(const char format[], ...);

void PM_GNU_PRINTF_ATTR(1,2)
pm_error (const char reason[], ...);       

int
pm_have_float_format(void);

/* Obsolete - use shhopt and user's manual instead */
void 
pm_usage (const char usage[]);             

NETPBM_EXPORT extern FILE* 
pm_openr (const char* const name);
         
NETPBM_EXPORT FILE*    
pm_openw (const char* const name);
         
NETPBM_EXPORT FILE *
pm_openr_seekable(const char name[]);

NETPBM_EXPORT void     
pm_close (FILE* const f);

NETPBM_EXPORT void 
pm_closer (FILE* const f);
          
NETPBM_EXPORT void      
pm_closew (FILE* const f);



void
pm_readchar(FILE * const ifP,
            char * const cP);

static __inline__ void
pm_readcharu(FILE *          const ifP,
             unsigned char * const cP) {
    pm_readchar(ifP, (char *) cP);
}

void
pm_writechar(FILE * const ofP,
             char   const c);

static __inline__ void
pm_writecharu(FILE *        const ofP,
              unsigned char const c) {
    pm_writechar(ofP, (char) c);
}

int
pm_readbigshort(FILE *  const ifP, 
                short * const sP);

static __inline__ int
pm_readbigshortu(FILE*            const ifP, 
                 unsigned short * const sP) {
    return pm_readbigshort(ifP, (short *) sP);
}

int
pm_writebigshort(FILE * const ofP,
                 short  const s);

static __inline__ int
pm_writebigshortu(FILE *          const ofP,
                  unsigned short  const s) {
    return pm_writebigshort(ofP, (short) s);
}

int
pm_readbiglong(FILE * const ifP, 
               long * const lP);

static __inline__ int
pm_readbiglongu(FILE *          const ifP,
                unsigned long * const lP) {
    return pm_readbiglong(ifP, (long *) lP);
}

int
pm_readbiglong2(FILE * const ifP, 
                int32_t * const lP);

static __inline__ int
pm_readbiglongu2(FILE *     const ifP,
                 uint32_t * const lP) {
    return pm_readbiglong2(ifP, (int32_t *) lP);
}

int
pm_writebiglong(FILE * const ofP,
                long   const l);

static __inline__ int
pm_writebiglongu(FILE *        const ofP,
                 unsigned long const l) {
    return pm_writebiglong(ofP, (long) l);
}

int
pm_readlittleshort(FILE  * const ifP,
                   short * const sP);

static __inline__ int
pm_readlittleshortu(FILE  *          const ifP,
                    unsigned short * const sP) {
    return pm_readlittleshort(ifP, (short *) sP);
}

int
pm_writelittleshort(FILE * const ofP,
                    short  const s);

static __inline__ int
pm_writelittleshortu(FILE *          const ofP,
                     unsigned short  const s) {
    return pm_writelittleshort(ofP, (short) s);
}

int
pm_readlittlelong(FILE * const ifP,
                  long * const lP);

static __inline__ int
pm_readlittlelongu(FILE *          const ifP,
                   unsigned long * const lP) {
    return pm_readlittlelong(ifP, (long *) lP);
}

int
pm_readlittlelong2(FILE *    const ifP,
                   int32_t * const lP);

static __inline__ int
pm_readlittlelong2u(FILE *     const ifP,
                    uint32_t * const lP) {
    return pm_readlittlelong2(ifP, (int32_t *) lP);
}

int
pm_writelittlelong(FILE * const ofP,
                   long   const l);

static __inline__ int
pm_writelittlelongu(FILE *        const ofP,
                    unsigned long const l) {
    return pm_writelittlelong(ofP, (long) l);
}

int 
pm_readmagicnumber(FILE * const ifP);

char* 
pm_read_unknown_size(FILE * const ifP, 
                     long * const buf);

short
pm_bs_short(short const s);

long
pm_bs_long(long const l);

unsigned int
pm_tell(FILE * const fileP);

void
pm_tell2(FILE *       const fileP, 
         void *       const fileposP,
         unsigned int const fileposSize);

void
pm_seek2(FILE *             const fileP, 
         const pm_filepos * const fileposP,
         unsigned int       const fileposSize);

void
pm_seek(FILE * const fileP, unsigned long filepos);

enum pm_check_code {
    PM_CHECK_OK,
    PM_CHECK_UNKNOWN_TYPE,
    PM_CHECK_TOO_LONG,
    PM_CHECK_UNCHECKABLE,
    PM_CHECK_TOO_SHORT
};

enum pm_check_type {
    PM_CHECK_BASIC
};

void
pm_check(FILE *               const file, 
         enum pm_check_type   const check_type, 
         pm_filepos           const need_raster_size,
         enum pm_check_code * const retval_p);

void
pm_drain(FILE *         const fileP,
         unsigned int   const limit,
         unsigned int * const bytesReadP);

char *
pm_arg0toprogname(const char arg0[]);

unsigned int
pm_randseed(void);

#ifdef __cplusplus
}
#endif


#endif
