/**************************************************************************
                               pmfileio.cxx
***************************************************************************
  This file contains fundamental file I/O stuff for libnetpbm.
  These are external functions, unlike 'fileio.c', but are not
  particular to any Netpbm format.
**************************************************************************/

#include <cstring>
#include <string>
#ifdef _WIN32
#include <io.h> // for close
#endif

#define _SVID_SOURCE
    /* Make sure P_tmpdir is defined in GNU libc 2.0.7 (_XOPEN_SOURCE 500
       does it in other libc's).  pm_config.h defines TMPDIR as P_tmpdir
       in some environments.
    */
#ifndef __APPLE__
#define _BSD_SOURCE    /* Make sure strdup is defined */
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 500    /* Make sure ftello, fseeko, strdup are defined */
#endif
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

#include "pm_config.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#if HAVE_IO_H
  #include <io.h>  /* For mktemp */
#endif

#include "pm_c_util.h"
#include "mallocvar.h"


#include "pm.h"



/* File open/close that handles "-" as stdin/stdout and checks errors. */

FILE *
pm_openr(const char * const name) {
    FILE * f;

    if (!strcmp(name, "-"))
        f = stdin;
    else {
        f = fopen(name, "rb");
        if (f == NULL) 
            pm_error("Unable to open file '%s' for reading.  "
                     "fopen() returns errno %d (%s)", 
                     name, errno, strerror(errno));
    }
    return f;
}



FILE *
pm_openw(const char * const name) {
    FILE * f;

    if (!strcmp(name, "-"))
        f = stdout;
    else {
        f = fopen(name, "wb");
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
    const char * tmpdir;
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



static int
tempFileOpenFlags(void) {
/*----------------------------------------------------------------------------
  Open flags (argument to open()) suitable for a new temporary file  
-----------------------------------------------------------------------------*/
    int retval;

    retval = 0
        | O_CREAT
        | O_RDWR
#if !MSVCRT
        | O_EXCL
#endif
#if MSVCRT
        | O_BINARY
#endif
        ;

    return retval;
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

static int
mkstempx(char * const filenameBuffer) {
/*----------------------------------------------------------------------------
  This is meant to be equivalent to POSIX mkstemp().

  On some old systems, mktemp() is a security hazard that allows a hacker
  to read or write our temporary file or cause us to read or write some
  unintended file.  On other systems, mkstemp() does not exist.

  A Windows/mingw environment is one which doesn't have mkstemp()
  (2006.06.15).

  We assume that if a system doesn't have mkstemp() that its mktemp()
  is safe, or that the total situation is such that the problems of
  mktemp() are not a problem for the user.
-----------------------------------------------------------------------------*/
    int retval;
    int fd;
    unsigned int attempts;
    bool gotFile;
    bool error;

    for (attempts = 0, gotFile = FALSE, error = FALSE;
         !gotFile && !error && attempts < 100;
         ++attempts) {

        int rc;
        rc = mkstemp(filenameBuffer);

        if (rc == -1)
            error = TRUE;
        else {
            int rc;

            rc = open(filenameBuffer, tempFileOpenFlags(),
                      PM_S_IWUSR | PM_S_IRUSR);

            if (rc >= 0) {
                fd = rc;
                gotFile = TRUE;
            } else {
                if (errno == EEXIST) {
                    /* We'll just have to keep trying */
                } else 
                    error = TRUE;
            }
        }
    }    
    if (gotFile)
        retval = fd;
    else
        retval = -1;

    return retval;
}



static int
mkstemp2(char * const filenameBuffer) {

#if HAVE_MKSTEMP
    if (0)
        mkstempx(NULL);  /* defeat compiler unused function warning */
    return mkstemp(filenameBuffer);
#else
    return mkstempx(filenameBuffer);
#endif
}



static void
makeTmpfileWithTemplate(std::string &filenameTemplate,
                        int *         const fdP,
                        const char ** const filenameP,
                        const char ** const errorP) {
    
    char * filenameBuffer;  /* malloc'ed */

    filenameBuffer = strdup(filenameTemplate.c_str());

    if (filenameBuffer == NULL)
        pm_error("Unable to allocate storage for temporary "
                    "file name");
    else {
        int rc;
        
        rc = mkstemp2(filenameBuffer);
        
        if (rc < 0)
            pm_error("Unable to create temporary file according to name "
                        "pattern '%s'.  mkstemp() failed with errno %d (%s)",
                        filenameTemplate.c_str(), errno, strerror(errno));
        else {
            *fdP = rc;
            *filenameP = filenameBuffer;
            *errorP = NULL;
        }
        if (*errorP)
            free((void *)filenameBuffer);
    }
}



void
pm_make_tmpfile_fd(int *         const fdP,
                   const char ** const filenameP) {

    const char * dirseparator;
    const char * error = NULL;
#if defined(_MSC_VER) && (_MSC_VER > 1800)
    char tmpdir_aux_win32[MAX_PATH + 1];
#else
    char *tmpdir_aux_win32;
#endif
    const char * tmpdir = tmpDir(tmpdir_aux_win32);

    if (tmpdir[strlen(tmpdir) - 1] == '/')
        dirseparator = "";
    else
        dirseparator = "/";

    std::string filenameTemplate;
    unsigned int fnamelen = strlen(pm_progname) + 10; /* "/" + "_XXXXXX\0" */

    filenameTemplate = std::string(tmpdir) + std::string(dirseparator) + std::string(pm_progname) + std::string("_XXXXXX  ");

    if (!filenameTemplate.length())
        pm_error("Unable to allocate storage for temporary file name");
    else {
        makeTmpfileWithTemplate(filenameTemplate, fdP, filenameP, &error);
    }
    if (error) {
        pm_error("%s", error);
        free((void *)error);
        pm_longjmp();
    }
}



void
pm_make_tmpfile(FILE **       const filePP,
                const char ** const filenameP) {

    int fd;

    pm_make_tmpfile_fd(&fd, filenameP);

    *filePP = fdopen(fd, "w+b");
    
    if (*filePP == NULL) {
        close(fd);
        unlink(*filenameP);
        free((void *)*filenameP);

        pm_error("Unable to create temporary file.  "
                 "fdopen() failed with errno %d (%s)",
                 errno, strerror(errno));
    }
}



bool const canUnlinkOpen = 
#if CAN_UNLINK_OPEN
    1
#else
    0
#endif
    ;



typedef struct UnlinkListEntry {
/*----------------------------------------------------------------------------
   This is an entry in the linked list of files to close and unlink as the
   program exits.
-----------------------------------------------------------------------------*/
    struct UnlinkListEntry * next;
    int                      fd;
    char                     fileName[1];  /* Actually variable length */
} UnlinkListEntry;

static UnlinkListEntry * unlinkListP;



static void
unlinkTempFiles(void) {
/*----------------------------------------------------------------------------
  Close and unlink (so presumably delete) the files in the list
  *unlinkListP.

  This is an atexit function.
-----------------------------------------------------------------------------*/
    while (unlinkListP) {
        UnlinkListEntry * const firstEntryP = unlinkListP;

        unlinkListP = unlinkListP->next;

        close(firstEntryP->fd);
        unlink(firstEntryP->fileName);

        free(firstEntryP);
    }
}



static UnlinkListEntry *
newUnlinkListEntry(const char * const fileName,
                   int          const fd) {

     UnlinkListEntry * const unlinkListEntryP = (UnlinkListEntry * const)
        malloc(sizeof(*unlinkListEntryP) + strlen(fileName) + 1);

    if (unlinkListEntryP) {
        strcpy(unlinkListEntryP->fileName, fileName);
        unlinkListEntryP->fd   = fd;
        unlinkListEntryP->next = NULL;
    }
    return unlinkListEntryP;
}



static void
addUnlinkListEntry(const char * const fileName,
                   int          const fd) {

    UnlinkListEntry * const unlinkListEntryP =
        newUnlinkListEntry(fileName, fd);

    if (unlinkListEntryP) {
        unlinkListEntryP->next = unlinkListP;
        unlinkListP = unlinkListEntryP;
    }
}



static void
scheduleUnlinkAtExit(const char * const fileName,
                     int          const fd) {
/*----------------------------------------------------------------------------
   Set things up to have the file unlinked as the program exits.

   This is messy and probably doesn't work in all situations; it is a hack
   to get Unix code essentially working on Windows, without messing up the
   code too badly for Unix.
-----------------------------------------------------------------------------*/
    static bool unlinkListEstablished = false;
    
    if (!unlinkListEstablished) {
        atexit(unlinkTempFiles);
        unlinkListP = NULL;
        unlinkListEstablished = true;
    }

    addUnlinkListEntry(fileName, fd);
}



static void
arrangeUnlink(const char * const fileName,
              int          const fd) {

    if (canUnlinkOpen)
        unlink(fileName);
    else
        scheduleUnlinkAtExit(fileName, fd);
}



FILE * 
pm_tmpfile(void) {

    FILE * fileP;
    const char * tmpfileNm = NULL;

    pm_make_tmpfile(&fileP, &tmpfileNm);

    arrangeUnlink(tmpfileNm, fileno(fileP));

    free((void *)tmpfileNm);

    return fileP;
}



int
pm_tmpfile_fd(void) {

    int fd;
    const char * tmpfileNm = NULL;

    pm_make_tmpfile_fd(&fd, &tmpfileNm);

    arrangeUnlink(tmpfileNm, fd);

    free((void *)tmpfileNm);

    return fd;
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



static unsigned char
getcNofail(FILE * const ifP) {

    int c;

    c = getc(ifP);

    if (c == EOF)
        abortWithReadError(ifP);

    return (unsigned char)c;
}



void
pm_readchar(FILE * const ifP,
            char * const cP) {
    
    *cP = (char)getcNofail(ifP);
}



void
pm_writechar(FILE * const ofP,
             char   const c) {

    putc(c, ofP);
}



int
pm_readbigshort(FILE *  const ifP, 
                short * const sP) {

    unsigned short s;

    s  = getcNofail(ifP) << 8;
    s |= getcNofail(ifP) << 0;

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

    unsigned long l;

    l  = getcNofail(ifP) << 24;
    l |= getcNofail(ifP) << 16;
    l |= getcNofail(ifP) <<  8;
    l |= getcNofail(ifP) <<  0;

    *lP = l;

    return 0;
}



int
pm_readbiglong2(FILE *    const ifP,
                int32_t * const lP) {
    int rc;
    long l;

    rc = pm_readbiglong(ifP, &l);

    assert((int32_t)l == l);

    *lP = (int32_t)l;

    return rc;
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
    unsigned short s;

    s  = getcNofail(ifP) << 0;
    s |= getcNofail(ifP) << 8;

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
    unsigned long l;

    l  = getcNofail(ifP) <<  0;
    l |= getcNofail(ifP) <<  8;
    l |= getcNofail(ifP) << 16;
    l |= getcNofail(ifP) << 24;

    *lP = l;

    return 0;
}



int
pm_readlittlelong2(FILE *    const ifP,
                   int32_t * const lP) {
    int rc;
    long l;

    rc = pm_readlittlelong(ifP, &l);

    assert((int32_t)l == l);

    *lP = (int32_t)l;

    return rc;
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

   Oliver Trepte, oliver@fysik4.kth.se, 930613
*/

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
    void *array;
    mallocProduct(&array, nalloc , sizeof(char));
    buf = (char *)array;

    if (!buf)
        pm_error("Failed to allocate %lu bytes for read buffer",
                 (unsigned long) nalloc);

    eof = FALSE;  /* initial value */

    while(!eof) {
        int val;

        if (*nread >= nalloc) { /* We need a larger buffer */
            if (nalloc > PM_MAX_BUF_INC)
                nalloc += PM_MAX_BUF_INC;
            else
                nalloc += nalloc;
	    array = buf;
	    reallocProduct(&array, nalloc, sizeof(buf[0]));
	    if (!array && buf)
		    free(buf);
	    buf = (char *)array;
            if (!buf)
                pm_error("Failed to allocate %lu bytes for read buffer",
                         (unsigned long) nalloc);
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
    uint32_t l;
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
                 fileposSize, (unsigned int)sizeof(pm_filepos),
                 (unsigned int) sizeof(long));
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
                 fileposSize, (unsigned int)sizeof(pm_filepos),
                 (unsigned int) sizeof(long));
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



void
pm_drain(FILE *         const fileP,
         unsigned int   const limit,
         unsigned int * const bytesReadP) {
/*----------------------------------------------------------------------------
  Read bytes from *fileP until EOF and return as *bytesReadP how many there
  were.

  But don't read any more than 'limit'.

  This is a good thing to call after reading an input file to be sure you
  didn't leave some input behind, which could mean you didn't properly
  interpret the file.
-----------------------------------------------------------------------------*/
    unsigned int bytesRead;
    bool eof;

    for (bytesRead = 0, eof = false; !eof && bytesRead < limit;) {

        int rc;

        rc = fgetc(fileP);

        eof = (rc == EOF);
        if (!eof)
            ++bytesRead;
    }
    *bytesReadP = bytesRead;
}
