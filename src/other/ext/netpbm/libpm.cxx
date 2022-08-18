/**************************************************************************
                                  libpm.c
***************************************************************************
  This file contains fundamental libnetpbm services.

  Some of the subroutines in this library are intended and documented
  for use by Netpbm users, but most of them are just used by other
  Netpbm library subroutines.
**************************************************************************/

#include <string>

#ifndef __APPLE__
#define _BSD_SOURCE          /* Make sure strdup is in string.h */
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500    /* Make sure ftello, fseeko are defined */
#endif

#define _SVID_SOURCE
    /* Make sure P_tmpdir is defined in GNU libc 2.0.7 (_XOPEN_SOURCE 500
       does it in other libc's).  pm_config.h defines TMPDIR as P_tmpdir
       in some environments.
    */

#define _LARGEFILE_SOURCE 1  /* Make sure ftello, fseeko are defined */
#define _LARGEFILE64_SOURCE 1 
#define _FILE_OFFSET_BITS 64
    /* This means ftello() is really ftello64() and returns a 64 bit file
       position.  Unless the C library doesn't have ftello64(), in which 
       case ftello() is still just ftello().

       Likewise for all the other C library file functions.

       And off_t and fpos_t are 64 bit types instead of 32.  Consequently,
       pm_filepos_t might be 64 bits instead of 32.
    */
#define _LARGE_FILES  
    /* This does for AIX what _FILE_OFFSET_BITS=64 does for GNU */
#define _LARGE_FILE_API
    /* This makes the the x64() functions available on AIX */

#define CHARMALLOCARRAY(arrayName, nElements) do { \
    void * array; \
    mallocProduct(&array, nElements, sizeof(arrayName[0])); \
    arrayName = (char *)array; \
} while (0)

#define ACHARMALLOCARRAY(arrayName, nElements) do { \
    void * array; \
    mallocProduct(&array, nElements, sizeof(arrayName[0])); \
    arrayName = (char **)array; \
} while (0)

#define CHARREALLOCARRAY(arrayName, nElements) { \
	    void * array; \
	    array = arrayName; \
	    reallocProduct(&array, nElements, sizeof(arrayName[0])); \
	    arrayName = (char *)array; \
} while (0)

#define CHARREALLOCARRAY_NOFAIL(arrayName, nElements) \
	do { \
		    CHARREALLOCARRAY(arrayName, nElements); \
		    if ((arrayName) == NULL) \
		        abort(); \
	} while(0)


#ifdef WIN32
# include <io.h>
#endif

#include "pm_config.h"

extern "C" {
#include <assert.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <time.h>
#include <limits.h>
#if HAVE_FORK
#include <sys/wait.h>
#endif
#include <sys/types.h>

#include "pm_c_util.h"
#include "mallocvar.h"
#include "version.h"


#include "compile.h"

#include "pm.h"

/* The following are set by pm_init(), then used by subsequent calls to other
   pm_xxx() functions.
   */
const char * pm_progname;

int pm_plain_output;
    /* Boolean: programs should produce output in plain format */

bool pm_showmessages;  
    /* Programs should display informational messages (because the user didn't
       specify the --quiet option).
    */
static jmp_buf * pm_jmpbufP = NULL;
    /* A description of the point to which the program should hyperjump
       if a libnetpbm function encounters an error (libnetpbm functions
       don't normally return in that case).

       User sets this to something in his own extra-library context.
       Libnetpbm routines that have something that needs to be cleaned up
       preempt it.

       NULL, which is the default value, means when a libnetpbm function
       encounters an error, it causes the process to exit.
    */
static pm_usererrormsgfn * userErrorMsgFn = NULL;
    /* A function to call to issue an error message.

       NULL means use the library default: print to Standard Error
    */

static pm_usermessagefn * userMessageFn = NULL;
    /* A function to call to issue an error message.

       NULL means use the library default: print to Standard Error
    */



void
pm_setjmpbuf(jmp_buf * const jmpbufP) {
    pm_jmpbufP = jmpbufP;
}



void
pm_setjmpbufsave(jmp_buf *  const jmpbufP,
                 jmp_buf ** const oldJmpbufPP) {

    *oldJmpbufPP = pm_jmpbufP;
    pm_jmpbufP = jmpbufP;
}



void
pm_longjmp(void) {

    if (pm_jmpbufP)
        longjmp(*pm_jmpbufP, 1);
    else
        exit(1);
}



void
pm_fork(int *         const iAmParentP,
        pid_t *       const childPidP,
        const char ** const errorP) {
/*----------------------------------------------------------------------------
   Same as POSIX fork, except with a nicer interface and works
   (fails cleanly) on systems that don't have POSIX fork().
-----------------------------------------------------------------------------*/
#if HAVE_FORK
    int rc;

    rc = fork();

    if (rc < 0) {
        pm_asprintf(errorP, "Failed to fork a process.  errno=%d (%s)",
                    errno, strerror(errno));
    } else {
        *errorP = NULL;

        if (rc == 0) {
            *iAmParentP = FALSE;
        } else {
            *iAmParentP = TRUE;
            *childPidP = rc;
        }
    }
#else
    pm_error("Cannot fork a process, because this system does "
                "not have POSIX fork()");
#endif
}



void
pm_waitpid(pid_t         const pid,
           int *         const statusP,
           int           const options,
           pid_t *       const exitedPidP,
           const char ** const errorP) {

#if HAVE_FORK
    pid_t rc;
    rc = waitpid(pid, statusP, options);
    if (rc == (pid_t)-1) {
        pm_asprintf(errorP, "Failed to wait for process exit.  "
                    "waitpid() errno = %d (%s)",
                    errno, strerror(errno));
    } else {
        *exitedPidP = rc;
        *errorP = NULL;
    }
#else
    pm_error("INTERNAL ERROR: Attempt to wait for a process we created on "
             "a system on which we can't create processes");
#endif
}



void
pm_waitpidSimple(pid_t const pid) {

    int status;
    pid_t exitedPid;
    const char * error = NULL;

    pm_waitpid(pid, &status, 0, &exitedPid, &error);

    if (error) {
        pm_error("%s", error);
        free((void *)error);
        pm_longjmp();
    } else {
        assert(exitedPid != 0);
    }
}



void
pm_setusererrormsgfn(pm_usererrormsgfn * fn) {

    userErrorMsgFn = fn;
}



void
pm_setusermessagefn(pm_usermessagefn * fn) {

    userMessageFn = fn;
}



void
pm_usage(const char usage[]) {
    pm_error("usage:  %s %s", pm_progname, usage);
}



void PM_GNU_PRINTF_ATTR(1,2)
pm_message(const char format[], ...) {

    va_list args;

    va_start(args, format);

    if (pm_showmessages) {
        fprintf(stderr, "%s: ", pm_progname);
        vfprintf(stderr, format, args);
        fputc('\n', stderr);
    }
    va_end(args);
}

void
pm_perror(const char reason[] ) {

    if (reason != NULL && strlen(reason) != 0)
        pm_error("%s - errno=%d (%s)", reason, errno, strerror(errno));
    else
        pm_error("Something failed with errno=%d (%s)", 
                 errno, strerror(errno));
}

void PM_GNU_PRINTF_ATTR(1,2)
pm_error(const char format[], ...) {
    va_list args;

    va_start(args, format);

    fprintf(stderr, "%s: ", pm_progname);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);

    pm_longjmp();
}



int
pm_have_float_format(void) {
/*----------------------------------------------------------------------------
  Return 1 if %f, %e, and %g work in format strings for pm_message, etc.;
  0 otherwise.

  Where they don't "work," that means the specifier just appears itself in
  the formatted strings, e.g. "the number is g".
-----------------------------------------------------------------------------*/
    return 1;
}



static void *
mallocz(size_t const size) {

    return malloc(MAX(1, size));
}



char *
pm_allocrow(unsigned int const cols,
            unsigned int const size) {

    unsigned char * itrow;

    if (cols != 0 && UINT_MAX / cols < size)
        pm_error("Arithmetic overflow multiplying %u by %u to get the "
                 "size of a row to allocate.", cols, size);

    itrow = (unsigned char *)mallocz(cols * size);
    if (itrow == NULL)
        pm_error("out of memory allocating a row");

    return (char *)itrow;
}



void
pm_freerow(char * const itrow) {
    free(itrow);
}



char**
pm_allocarray(int const cols, int const rows, int const size )  {
/*----------------------------------------------------------------------------
   Allocate an array of 'rows' rows of 'cols' columns each, with each
   element 'size' bytes.

   We use a special format where we tack on an extra element to the row
   index to indicate the format of the array.

   We have two ways of allocating the space: fragmented and
   unfragmented.  In both, the row index (plus the extra element) is
   in one block of memory.  In the fragmented format, each row is
   also in an independent memory block, and the extra row pointer is
   NULL.  In the unfragmented format, all the rows are in a single
   block of memory called the row heap and the extra row pointer is
   the address of that block.

   We use unfragmented format if possible, but if the allocation of the
   row heap fails, we fall back to fragmented.
-----------------------------------------------------------------------------*/
    char** rowIndex;
    char * rowheap;

    ACHARMALLOCARRAY(rowIndex, rows + 1);
    if (rowIndex == NULL)
        pm_error("out of memory allocating row index (%u rows) for an array",
                 rows);

    if (cols != 0 && rows != 0 && UINT_MAX / cols / rows < (unsigned int)size)
        /* Too big even to request the memory ! */
        rowheap = NULL;
    else
        rowheap = (char *)malloc((unsigned int)rows * cols * size);

    if (rowheap == NULL) {
        /* We couldn't get the whole heap in one block, so try fragmented
           format.
        */
        int row;
        
        rowIndex[rows] = NULL;   /* Declare it fragmented format */

        for (row = 0; row < rows; ++row) {
            rowIndex[row] = pm_allocrow(cols, size);
            if (rowIndex[row] == NULL)
                pm_error("out of memory allocating Row %u "
                         "(%u columns, %u bytes per tuple) "
                         "of an array", row, cols, size);
        }
    } else {
        /* It's unfragmented format */
        int row;
        rowIndex[rows] = rowheap;  /* Declare it unfragmented format */

        for (row = 0; row < rows; ++row)
            rowIndex[row] = &(rowheap[row * cols * size]);
    }
    return rowIndex;
}



void
pm_freearray(char ** const rowIndex, 
             int     const rows) {

    void * const rowheap = rowIndex[rows];

    if (rowheap != NULL)
        free(rowheap);
    else {
        int row;
        for (row = 0; row < rows; ++row)
            pm_freerow(rowIndex[row]);
    }
    free(rowIndex);
}



/* Case-insensitive keyword matcher. */

int
pm_keymatch(const char *       const strarg, 
            const char * const keywordarg, 
            int          const minchars) {
    int len;
    const char * keyword;
    const char * str;

    str = strarg;
    keyword = keywordarg;

    len = strlen( str );
    if ( len < minchars )
        return 0;
    while ( --len >= 0 )
        {
        register char c1, c2;

        c1 = *str++;
        c2 = *keyword++;
        if ( c2 == '\0' )
            return 0;
        if ( isupper( c1 ) )
            c1 = tolower( c1 );
        if ( isupper( c2 ) )
            c2 = tolower( c2 );
        if ( c1 != c2 )
            return 0;
        }
    return 1;
}


/* Log base two hacks. */

int
pm_maxvaltobits(int const maxval) {
    if ( maxval <= 1 )
        return 1;
    else if ( maxval <= 3 )
        return 2;
    else if ( maxval <= 7 )
        return 3;
    else if ( maxval <= 15 )
        return 4;
    else if ( maxval <= 31 )
        return 5;
    else if ( maxval <= 63 )
        return 6;
    else if ( maxval <= 127 )
        return 7;
    else if ( maxval <= 255 )
        return 8;
    else if ( maxval <= 511 )
        return 9;
    else if ( maxval <= 1023 )
        return 10;
    else if ( maxval <= 2047 )
        return 11;
    else if ( maxval <= 4095 )
        return 12;
    else if ( maxval <= 8191 )
        return 13;
    else if ( maxval <= 16383 )
        return 14;
    else if ( maxval <= 32767 )
        return 15;
    else if ( (long) maxval <= 65535L )
        return 16;
    else
        pm_error( "maxval of %d is too large!", maxval );
        return -1;  /* Should never come here */
}

int
pm_bitstomaxval(int const bits) {
    return ( 1 << bits ) - 1;
}


unsigned int PURE_FN_ATTR
pm_lcm(unsigned int const x, 
       unsigned int const y,
       unsigned int const z,
       unsigned int const limit) {
/*----------------------------------------------------------------------------
  Compute the least common multiple of 'x', 'y', and 'z'.  If it's bigger than
  'limit', though, just return 'limit'.
-----------------------------------------------------------------------------*/
    unsigned int biggest;
    unsigned int candidate;

    if (x == 0 || y == 0 || z == 0)
        pm_error("pm_lcm(): Least common multiple of zero taken.");

    biggest = MAX(x, MAX(y,z));

    candidate = biggest;
    while (((candidate % x) != 0 ||       /* not a multiple of x */
            (candidate % y) != 0 ||       /* not a multiple of y */
            (candidate % z) != 0 ) &&     /* not a multiple of z */
           candidate <= limit)
        candidate += biggest;

    if (candidate > limit) 
        candidate = limit;

    return candidate;
}



void
pm_init(const char * const progname,
        unsigned int const flags) {
/*----------------------------------------------------------------------------
   Initialize static variables that Netpbm library routines use.

   Any user of Netpbm library routines is expected to call this at the
   beginning of this program, before any other Netpbm library routines.

   A program may call this via pm_proginit() instead, though.
-----------------------------------------------------------------------------*/
    pm_setMessage(FALSE, NULL);

    pm_progname = progname;

#ifdef O_BINARY
#ifdef HAVE_SETMODE
    /* Set the stdin and stdout mode to binary.  This means nothing on Unix,
       but matters on Windows.
       
       Note that stdin and stdout aren't necessarily image files.  In
       particular, stdout is sometimes text for human consumption,
       typically printed on the terminal.  Binary mode isn't really
       appropriate for that case.  We do this setting here without
       any knowledge of how stdin and stdout are being used because it is
       easy.  But we do make an exception for the case that we know the
       file is a terminal, to get a little closer to doing the right
       thing.  
    */
    if (!isatty(0)) setmode(0,O_BINARY);  /* Standard Input */
    if (!isatty(1)) setmode(1,O_BINARY);  /* Standard Output */
#endif /* HAVE_SETMODE */
#endif /* O_BINARY */
}



static void
showVersion(void) {
    pm_message( "Using libnetpbm from Netpbm Version: %s", NETPBM_VERSION );
#if defined(COMPILE_TIME) && defined(COMPILED_BY)
    pm_message( "Compiled %s by user \"%s\"",
                COMPILE_TIME, COMPILED_BY );
#endif
#ifdef BSD
    pm_message( "BSD defined" );
#endif /*BSD*/
#ifdef SYSV
    pm_message( "SYSV defined" );
#endif /*SYSV*/
#ifdef MSDOS
    pm_message( "MSDOS defined" );
#endif /*MSDOS*/
#ifdef AMIGA
    pm_message( "AMIGA defined" );
#endif /* AMIGA */
    {
        const char * rgbdef;
        pm_message( "RGB_ENV='%s'", RGBENV );
        rgbdef = getenv(RGBENV);
        if( rgbdef )
            pm_message( "RGBENV= '%s' (env vbl set to '%s')", 
                        RGBENV, rgbdef );
        else
            pm_message( "RGBENV= '%s' (env vbl is unset)", RGBENV);
    }
}



static void
showNetpbmHelp(const char progname[]) {
/*----------------------------------------------------------------------------
  Tell the user where to get help for this program, assuming it is a Netpbm
  program (a program that comes with the Netpbm package, as opposed to a 
  program that just uses the Netpbm libraries).

  Tell him to go to the URL listed in the Netpbm configuration file.
  The Netpbm configuration file is the file named by the NETPBM_CONF
  environment variable, or /etc/netpbm if there is no such environment
  variable.

  If the configuration file doesn't exist or can't be read, or doesn't
  contain a DOCURL value, tell him to go to a hardcoded source for
  documentation.
-----------------------------------------------------------------------------*/
    const char * netpbmConfigFileName;
    FILE * netpbmConfigFile;
    char * docurl;

    if (getenv("NETPBM_CONF"))
        netpbmConfigFileName = getenv("NETPBM_CONF");
    else 
        netpbmConfigFileName = "/etc/netpbm";
    
    netpbmConfigFile = fopen(netpbmConfigFileName, "r");
    if (netpbmConfigFile == NULL) {
        pm_message("Unable to open Netpbm configuration file '%s'.  "
                   "Errno = %d (%s).  "
                   "Use the NETPBM_CONF environment variable "
                   "to control the identity of the Netpbm configuration file.",
                   netpbmConfigFileName,errno, strerror(errno));
        docurl = NULL;
    } else {
        docurl = NULL;  /* default */
        while (!feof(netpbmConfigFile) && !ferror(netpbmConfigFile)) {
            char line[80+1];
            fgets(line, sizeof(line), netpbmConfigFile);
            if (line[0] != '#') {
                sscanf(line, "docurl=%s", docurl);
            }
        }
        if (docurl == NULL)
            pm_message("No 'docurl=' line in Netpbm configuration file '%s'.",
                       netpbmConfigFileName);

        fclose(netpbmConfigFile);
    }
    if (docurl == NULL)
        pm_message("We have no reliable indication of where the Netpbm "
                   "documentation is, but try "
                   "http://netpbm.sourceforge.net or email "
                   "Bryan Henderson (bryanh@giraffe-data.com) for help.");
    else
        pm_message("This program is part of the Netpbm package.  Find "
                   "documentation for it at %s/%s\n", docurl, progname);
}



void
pm_proginit(int * const argcP, const char * argv[]) {
/*----------------------------------------------------------------------------
   Do various initialization things that all programs in the Netpbm package,
   and programs that emulate such programs, should do.

   This includes processing global options.

   This includes calling pm_init() to initialize the Netpbm libraries.
-----------------------------------------------------------------------------*/
    int argn, i;
    const char * progname;
    bool showmessages;
    bool show_version;
        /* We're supposed to just show the version information, then exit the
           program.
        */
    bool show_help;
        /* We're supposed to just tell user where to get help, then exit the
           program.
        */
    
    /* Extract program name. */
#ifdef VMS
    progname = vmsProgname(argcP, argv);
#else
    progname = strrchr( argv[0], '/');
#endif
    if (progname == NULL)
        progname = argv[0];
    else
        ++progname;

    pm_init(progname, 0);

    /* Check for any global args. */
    showmessages = TRUE;
    show_version = FALSE;
    show_help = FALSE;
    pm_plain_output = FALSE;
    for (argn = i = 1; argn < *argcP; ++argn) {
        if (pm_keymatch(argv[argn], "-quiet", 6) ||
            pm_keymatch(argv[argn], "--quiet", 7)) 
            showmessages = FALSE;
        else if (pm_keymatch(argv[argn], "-version", 8) ||
                   pm_keymatch(argv[argn], "--version", 9)) 
            show_version = TRUE;
        else if (pm_keymatch(argv[argn], "-help", 5) ||
                 pm_keymatch(argv[argn], "--help", 6) ||
                 pm_keymatch(argv[argn], "-?", 2)) 
            show_help = TRUE;
        else if (pm_keymatch(argv[argn], "-plain", 6) ||
                 pm_keymatch(argv[argn], "--plain", 7))
            pm_plain_output = TRUE;
        else
            argv[i++] = argv[argn];
    }
    *argcP=i;

    pm_setMessage((unsigned int) showmessages, NULL);

    if (show_version) {
        showVersion();
        exit( 0 );
    } else if (show_help) {
        pm_error("Use 'man %s' for help.", progname);
        /* If we can figure out a way to distinguish Netpbm programs from 
           other programs using the Netpbm libraries, we can do better here.
        */
        if (0)
            showNetpbmHelp(progname);
        exit(0);
    }
}


void
pm_setMessage(int const newState, int * const oldStateP) {
    
    if (oldStateP)
        *oldStateP = pm_showmessages;

    pm_showmessages = !!newState;
}


char *
pm_arg0toprogname(const char arg0[]) {
/*----------------------------------------------------------------------------
   Given a value for argv[0] (a command name or file name passed to a 
   program in the standard C calling sequence), return the name of the
   Netpbm program to which is refers.

   In the most ordinary case, this is simply the argument itself.

   But if the argument contains a slash, it is the part of the argument 
   after the last slash, and if there is a .exe on it (as there is for
   DJGPP), that is removed.

   The return value is in static storage within.  It is null-terminated,
   but truncated at 64 characters.
-----------------------------------------------------------------------------*/
    static char retval[64+1];
    const char *slash_pos;

    /* Chop any directories off the left end */
    slash_pos = strrchr(arg0, '/');

    if (slash_pos == NULL) {
        strncpy(retval, arg0, sizeof(retval));
        retval[sizeof(retval)-1] = '\0';
    } else {
        strncpy(retval, slash_pos +1, sizeof(retval));
        retval[sizeof(retval)-1] = '\0';
    }

    /* Chop any .exe off the right end */
    if (strlen(retval) >= 4 && strcmp(retval+strlen(retval)-4, ".exe") == 0)
        retval[strlen(retval)-4] = 0;

    return(retval);
}



/* File open/close that handles "-" as stdin/stdout and checks errors. */

FILE*
pm_openr(const char * const name) {
    FILE* f;

    if (strcmp(name, "-") == 0)
        f = stdin;
    else {
#ifndef VMS
        f = fopen(name, "rb");
#else
        f = fopen(name, "r", "ctx=stm");
#endif
        if (f == NULL) 
            pm_error("Unable to open file '%s' for reading.  "
                     "fopen() returns errno %d (%s)", 
                     name, errno, strerror(errno));
    }
    return f;
}



FILE*
pm_openw(const char * const name) {
    FILE* f;

    if (strcmp(name, "-") == 0)
        f = stdout;
    else {
#ifndef VMS
        f = fopen(name, "wb");
#else
        f = fopen(name, "w", "mbc=32", "mbf=2");  /* set buffer factors */
#endif
        if (f == NULL) 
            pm_error("Unable to open file '%s' for writing.  "
                     "fopen() returns errno %d (%s)", 
                     name, errno, strerror(errno));
    }
    return f;
}



static const char *
tmpDir(char *tmpdir_aux_win32) {
/*----------------------------------------------------------------------------
   Return the name of the directory in which we should create temporary
   files.

   The name is a constant in static storage.
-----------------------------------------------------------------------------*/
    const char *tmpdir;
        /* running approximation of the result */

    tmpdir = getenv("TMPDIR");   /* Unix convention */

    if (!tmpdir || strlen(tmpdir) == 0)
        tmpdir = getenv("TMP");  /* Windows convention */

    if (!tmpdir || strlen(tmpdir) == 0)
        tmpdir = getenv("TEMP"); /* Windows convention */

    if (!tmpdir || strlen(tmpdir) == 0) {
#if defined(_MSC_VER) && (_MSC_VER > 1800)
        GetTempPathA(MAX_PATH + 1, tmpdir_aux_win32);
        tmpdir = tmpdir_aux_win32;
#else
        tmpdir = TMPDIR;
#endif
    }

    return tmpdir;
}

#ifndef HAVE_MKSTEMP

static int
mkstemp(char *file_template)
{
    int fd = -1;
    int counter = 0;
    size_t i;
    size_t start, end;

    static const char replace[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static int replacelen = sizeof(replace) - 1;

    if (!file_template || file_template[0] == '\0')
        return -1;

    /* identify the replacement suffix */
    start = end = strlen(file_template)-1;
    for (i=strlen(file_template)-1; i>=0; i--) {
        if (file_template[i] != 'X') {
            break;
        }
        end = i;
    }

    do {
        /* replace the template with random chars */
        srand(time(NULL));
        for (i=start; i>=end; i--) {
            file_template[i] = replace[(int)(replacelen * ((double)rand() / (double)RAND_MAX))];
        }
#ifdef WIN32
        fd = _open(file_template, O_CREAT | O_EXCL | O_TRUNC | O_RDWR | O_TEMPORARY, S_IRUSR | S_IWUSR);
#else
        fd = open(file_template, O_CREAT | O_EXCL | O_TRUNC | O_RDWR | O_TEMPORARY, S_IRUSR | S_IWUSR);
#endif
    } while ((fd == -1) && (counter++ < 1000));

    return fd;
}
#endif

} /* extern "C" */

extern "C" void
pm_make_tmpfile(FILE **       const filePP,
                const char ** const filenameP) {

    int fd;
    FILE * fileP;
    std::string filenameTemplate;
    char * filenameBuffer;  /* malloc'ed */
    unsigned int fnamelen;
    const char * tmpdir;
    const char * dirseparator;

#if defined(_MSC_VER) && (_MSC_VER > 1800)
    char tmpdir_aux_win32[MAX_PATH + 1];
#else
    char *tmpdir_aux_win32;
#endif

    fnamelen = strlen (pm_progname) + 10; /* "/" + "_XXXXXX\0" */

    tmpdir = tmpDir(tmpdir_aux_win32);

    if (tmpdir[strlen(tmpdir) - 1] == '/')
        dirseparator = "";
    else
        dirseparator = "/";

    filenameTemplate = std::string(tmpdir) + std::string(dirseparator) + std::string(pm_progname) + std::string("_XXXXXX");

    filenameBuffer = strdup(filenameTemplate.c_str());

    fd = mkstemp(filenameBuffer);

    if (fd < 0)
        pm_error("Unable to create temporary file according to name "
                 "pattern '%s'.  mkstemp() failed with "
                 "errno %d (%s)", filenameTemplate.c_str(), errno, strerror(errno));
    else {
        fileP = fdopen(fd, "w+b");

        if (fileP == NULL)
            pm_error("Unable to create temporary file.  fdopen() failed "
                     "with errno %d (%s)", errno, strerror(errno));
    }

    *filenameP = filenameBuffer;
    *filePP = fileP;
}

extern "C" {

FILE * 
pm_tmpfile(void) {

    FILE * fileP;
    const char * tmpfile;

    pm_make_tmpfile(&fileP, &tmpfile);

    unlink(tmpfile);

    free((void *)tmpfile);

    return fileP;
}



FILE *
pm_openr_seekable(const char name[]) {
/*----------------------------------------------------------------------------
  Open the file named by name[] such that it is seekable (i.e. it can be
  rewound and read in multiple passes with fseek()).

  If the file is actually seekable, this reduces to the same as
  pm_openr().  If not, we copy the named file to a temporary file
  and return that file's stream descriptor.

  We use a file that the operating system recognizes as temporary, so
  it picks the filename and deletes the file when Caller closes it.
-----------------------------------------------------------------------------*/
    int stat_rc;
    int seekable;  /* logical: file is seekable */
    struct stat statbuf;
    FILE * original_file;
    FILE * seekable_file;

    original_file = pm_openr((char *) name);

    /* I would use fseek() to determine if the file is seekable and 
       be a little more general than checking the type of file, but I
       don't have reliable information on how to do that.  I have seen
       streams be partially seekable -- you can, for example seek to
       0 if the file is positioned at 0 but you can't actually back up
       to 0.  I have seen documentation that says the errno for an
       unseekable stream is EBADF and in practice seen ESPIPE.

       On the other hand, regular files are always seekable and even if
       some other file is, it doesn't hurt much to assume it isn't.
    */

    stat_rc = fstat(fileno(original_file), &statbuf);
    if (stat_rc == 0 && S_ISREG(statbuf.st_mode))
        seekable = TRUE;
    else 
        seekable = FALSE;

    if (seekable) {
        seekable_file = original_file;
    } else {
        seekable_file = pm_tmpfile();
        
        /* Copy the input into the temporary seekable file */
        while (!feof(original_file) && !ferror(original_file) 
               && !ferror(seekable_file)) {
            char buffer[4096];
            int bytes_read;
            bytes_read = fread(buffer, 1, sizeof(buffer), original_file);
            fwrite(buffer, 1, bytes_read, seekable_file);
        }
        if (ferror(original_file))
            pm_error("Error reading input file into temporary file.  "
                     "Errno = %s (%d)", strerror(errno), errno);
        if (ferror(seekable_file))
            pm_error("Error writing input into temporary file.  "
                     "Errno = %s (%d)", strerror(errno), errno);
        pm_close(original_file);
        {
            int seek_rc;
            seek_rc = fseek(seekable_file, 0, SEEK_SET);
            if (seek_rc != 0)
                pm_error("fseek() failed to rewind temporary file.  "
                         "Errno = %s (%d)", strerror(errno), errno);
        }
    }
    return seekable_file;
}



void
pm_close(FILE * const f) {
    fflush(f);
    if (ferror(f))
        pm_message("A file read or write error occurred at some point");
    if (f != stdin)
        if (fclose(f) != 0)
            pm_error("close of file failed with errno %d (%s)",
                     errno, strerror(errno));
}



/* The pnmtopng package uses pm_closer() and pm_closew() instead of 
   pm_close(), apparently because the 1999 Pbmplus package has them.
   I don't know what the difference is supposed to be.
*/

void
pm_closer(FILE * const f) {
    pm_close(f);
}



void
pm_closew(FILE * const f) {
    pm_close(f);
}



/* Endian I/O.

   Before Netpbm 10.27 (March 2005), these would return failure on EOF
   or I/O failure.  For backward compatibility, they still have the return
   code, but it is always zero and the routines abort the program in case
   of EOF or I/O failure.  A program that wants to handle failure differently
   must use lower level (C library) interfaces.  But that level of detail
   is uncharacteristic of a Netpbm program; the ease of programming that
   comes with not checking a return code is more Netpbm.

   It is also for historical reasons that these return signed values,
   when clearly unsigned would make more sense.
*/



static void
abortWithReadError(FILE * const ifP) {

    if (feof(ifP))
        pm_error("Unexpected end of input file");
    else
        pm_error("Error (not EOF) reading file.");
}



void
pm_readchar(FILE * const ifP,
            char * const cP) {
    
    int c;

    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);

    *cP = c;
}



void
pm_writechar(FILE * const ofP,
             char   const c) {

    putc(c, ofP);
}



int
pm_readbigshort(FILE *  const ifP, 
                short * const sP) {
    int c;

    unsigned short s;

    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    s = (c & 0xff) << 8;
    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    s |= c & 0xff;

    *sP = s;

    return 0;
}



int
pm_writebigshort(FILE * const ofP, 
                 short  const s) {

    putc((s >> 8) & 0xff, ofP);
    putc(s & 0xff, ofP);

    return 0;
}



int
pm_readbiglong(FILE * const ifP, 
               long * const lP) {

    int c;
    unsigned long l;

    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    l = c << 24;
    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    l |= c << 16;
    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    l |= c << 8;
    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    l |= c;

    *lP = l;

    return 0;
}



int
pm_writebiglong(FILE * const ofP, 
                long   const l) {

    putc((l >> 24) & 0xff, ofP);
    putc((l >> 16) & 0xff, ofP);
    putc((l >>  8) & 0xff, ofP);
    putc((l >>  0) & 0xff, ofP);

    return 0;
}



int
pm_readlittleshort(FILE *  const ifP, 
                   short * const sP) {
    int c;
    unsigned short s;

    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    s = c & 0xff;

    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    s |= (c & 0xff) << 8;

    *sP = s;

    return 0;
}



int
pm_writelittleshort(FILE * const ofP, 
                    short  const s) {

    putc((s >> 0) & 0xff, ofP);
    putc((s >> 8) & 0xff, ofP);

    return 0;
}



int
pm_readlittlelong(FILE * const ifP, 
                  long * const lP) {
    int c;
    unsigned long l;

    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    l = c;
    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    l |= c << 8;
    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    l |= c << 16;
    c = getc(ifP);
    if (c == EOF)
        abortWithReadError(ifP);
    l |= c << 24;

    *lP = l;

    return 0;
}



int
pm_writelittlelong(FILE * const ofP, 
                   long   const l) {

    putc((l >>  0) & 0xff, ofP);
    putc((l >>  8) & 0xff, ofP);
    putc((l >> 16) & 0xff, ofP);
    putc((l >> 24) & 0xff, ofP);

    return 0;
}



int 
pm_readmagicnumber(FILE * const ifP) {

    int ich1, ich2;

    ich1 = getc(ifP);
    ich2 = getc(ifP);
    if (ich1 == EOF || ich2 == EOF)
        pm_error( "Error reading magic number from Netpbm image stream.  "
                  "Most often, this "
                  "means your input file is empty." );

    return ich1 * 256 + ich2;
}



/* Read a file of unknown size to a buffer. Return the number of bytes
   read. Allocate more memory as we need it. The calling routine has
   to free() the buffer.

   Oliver Trepte, oliver@fysik4.kth.se, 930613 */

#define PM_BUF_SIZE 16384      /* First try this size of the buffer, then
                                   double this until we reach PM_MAX_BUF_INC */
#define PM_MAX_BUF_INC 65536   /* Don't allocate more memory in larger blocks
                                   than this. */

char *
pm_read_unknown_size(FILE * const file, 
                     long * const nread) {
    long nalloc;
    char * buf;
    bool eof;

    *nread = 0;
    nalloc = PM_BUF_SIZE;
    CHARMALLOCARRAY(buf, nalloc);

    eof = FALSE;  /* initial value */

    while(!eof) {
        int val;

        if (*nread >= nalloc) { /* We need a larger buffer */
            if (nalloc > PM_MAX_BUF_INC)
                nalloc += PM_MAX_BUF_INC;
            else
                nalloc += nalloc;
            CHARREALLOCARRAY_NOFAIL(buf, nalloc);
        }

        val = getc(file);
        if (val == EOF)
            eof = TRUE;
        else 
            buf[(*nread)++] = val;
    }
    return buf;
}



union cheat {
    uint32n l;
    short s;
    unsigned char c[4];
};



short
pm_bs_short(short const s) {
    union cheat u;
    unsigned char t;

    u.s = s;
    t = u.c[0];
    u.c[0] = u.c[1];
    u.c[1] = t;
    return u.s;
}



long
pm_bs_long(long const l) {
    union cheat u;
    unsigned char t;

    u.l = l;
    t = u.c[0];
    u.c[0] = u.c[3];
    u.c[3] = t;
    t = u.c[1];
    u.c[1] = u.c[2];
    u.c[2] = t;
    return u.l;
}



void
pm_tell2(FILE *       const fileP, 
         void *       const fileposP,
         unsigned int const fileposSize) {
/*----------------------------------------------------------------------------
   Return the current file position as *filePosP, which is a buffer
   'fileposSize' bytes long.  Abort the program if error, including if
   *fileP isn't a file that has a position.
-----------------------------------------------------------------------------*/
    /* Note: FTELLO() is either ftello() or ftell(), depending on the
       capabilities of the underlying C library.  It is defined in
       pm_config.h.  ftello(), in turn, may be either ftell() or
       ftello64(), as implemented by the C library.
    */
    pm_filepos const filepos = FTELLO(fileP);
    if (filepos < 0)
        pm_error("ftello() to get current file position failed.  "
                 "Errno = %s (%d)\n", strerror(errno), errno);

    if (fileposSize == sizeof(pm_filepos)) {
        pm_filepos * const fileposP_filepos = (pm_filepos * const)fileposP;
        *fileposP_filepos = filepos;
    } else if (fileposSize == sizeof(long)) {
        if (sizeof(pm_filepos) > sizeof(long) &&
            filepos >= (pm_filepos) 1 << (sizeof(long)*8))
            pm_error("File size is too large to represent in the %u bytes "
                     "that were provided to pm_tell2()", fileposSize);
        else {
            long * const fileposP_long = (long * const)fileposP;
            *fileposP_long = (long)filepos;
        }
    } else
        pm_error("File position size passed to pm_tell() is invalid: %u.  "
                 "Valid sizes are %u and %u", 
                 fileposSize, sizeof(pm_filepos), sizeof(long));
}



unsigned int
pm_tell(FILE * const fileP) {
    
    long filepos;

    pm_tell2(fileP, &filepos, sizeof(filepos));

    return filepos;
}



void
pm_seek2(FILE *             const fileP, 
         const pm_filepos * const fileposP,
         unsigned int       const fileposSize) {
/*----------------------------------------------------------------------------
   Position file *fileP to position *fileposP.  Abort if error, including
   if *fileP isn't a seekable file.
-----------------------------------------------------------------------------*/
    if (fileposSize == sizeof(pm_filepos)) 
        /* Note: FSEEKO() is either fseeko() or fseek(), depending on the
           capabilities of the underlying C library.  It is defined in
           pm_config.h.  fseeko(), in turn, may be either fseek() or
           fseeko64(), as implemented by the C library.
        */
        FSEEKO(fileP, *fileposP, SEEK_SET);
    else if (fileposSize == sizeof(long)) {
        long const fileposLong = *(long *)fileposP;
        fseek(fileP, fileposLong, SEEK_SET);
    } else
        pm_error("File position size passed to pm_seek() is invalid: %u.  "
                 "Valid sizes are %u and %u", 
                 fileposSize, sizeof(pm_filepos), sizeof(long));
}



void
pm_seek(FILE * const fileP, unsigned long filepos) {
/*----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/

    pm_filepos fileposBuff;

    fileposBuff = filepos;

    pm_seek2(fileP, &fileposBuff, sizeof(fileposBuff));
}



void
pm_nextimage(FILE * const file, int * const eofP) {
/*----------------------------------------------------------------------------
   Position the file 'file' to the next image in the stream, assuming it is
   now positioned just after the current image.  I.e. read off any white
   space at the end of the current image's raster.  Note that the raw formats
   don't permit such white space, but this routine tolerates it anyway, 
   because the plain formats do permit white space after the raster.

   Iff there is no next image, return *eofP == TRUE.

   Note that in practice, we will not normally see white space here in
   a plain PPM or plain PGM stream because the routine to read a
   sample from the image reads one character of white space after the
   sample in order to know where the sample ends.  There is not
   normally more than one character of white space (a newline) after
   the last sample in the raster.  But plain PBM is another story.  No white
   space is required between samples of a plain PBM image.  But the raster
   normally ends with a newline nonetheless.  Since the sample reading code
   will not have read that newline, it is there for us to read now.
-----------------------------------------------------------------------------*/
    bool eof;
    bool nonWhitespaceFound;

    eof = FALSE;
    nonWhitespaceFound = FALSE;

    while (!eof && !nonWhitespaceFound) {
        int c;
        c = getc(file);
        if (c == EOF) {
            if (feof(file)) 
                eof = TRUE;
            else
                pm_error("File error on getc() to position to image");
        } else {
            if (!isspace(c)) {
                int rc;

                nonWhitespaceFound = TRUE;

                /* Have to put the non-whitespace character back in
                   the stream -- it's part of the next image.  
                */
                rc = ungetc(c, file);
                if (rc == EOF) 
                    pm_error("File error doing ungetc() "
                             "to position to image.");
            }
        }
    }
    *eofP = eof;
}



void
pm_check(FILE *               const file, 
         enum pm_check_type   const check_type, 
         pm_filepos           const need_raster_size,
         enum pm_check_code * const retval_p) {
/*----------------------------------------------------------------------------
   This is not defined for use outside of libnetpbm.
-----------------------------------------------------------------------------*/
    struct stat statbuf;
    pm_filepos curpos;  /* Current position of file; -1 if none */
    int rc;

#ifdef LARGEFILEDEBUG
    pm_message("pm_filepos received by pm_check() is %u bytes.",
               sizeof(pm_filepos));
#endif
    /* Note: FTELLO() is either ftello() or ftell(), depending on the
       capabilities of the underlying C library.  It is defined in
       pm_config.h.  ftello(), in turn, may be either ftell() or
       ftello64(), as implemented by the C library.
    */
    curpos = FTELLO(file);
    if (curpos >= 0) {
        /* This type of file has a current position */
            
        rc = fstat(fileno(file), &statbuf);
        if (rc != 0) 
            pm_error("fstat() failed to get size of file, though ftello() "
                     "successfully identified\n"
                     "the current position.  Errno=%s (%d)",
                     strerror(errno), errno);
        else if (!S_ISREG(statbuf.st_mode)) {
            /* Not a regular file; we can't know its size */
            if (retval_p) *retval_p = PM_CHECK_UNCHECKABLE;
        } else {
            pm_filepos const have_raster_size = statbuf.st_size - curpos;
            
            if (have_raster_size < need_raster_size)
                pm_error("File has invalid format.  The raster should "
                         "contain %u bytes, but\n"
                         "the file ends after only %u bytes.",
                         (unsigned int) need_raster_size, 
                         (unsigned int) have_raster_size);
            else if (have_raster_size > need_raster_size) {
                if (retval_p) *retval_p = PM_CHECK_TOO_LONG;
            } else {
                if (retval_p) *retval_p = PM_CHECK_OK;
            }
        }
    } else
        if (retval_p) *retval_p = PM_CHECK_UNCHECKABLE;
}

} /* extern "C" */

