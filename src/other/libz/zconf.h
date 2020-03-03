/* zconf.h -- configuration of the zlib compression library
 * Copyright (C) 1995-2016 Jean-loup Gailly, Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ZCONF_H
#define ZCONF_H

/*
 * If you *really* need a unique prefix for all types and library functions,
 * compile with -DZ_PREFIX. The "standard" zlib should be compiled without it.
 * Even better than compiling with -DZ_PREFIX would be to use configure to set
 * this permanently in zconf.h using "./configure --zprefix".
 */
#ifdef Z_PREFIX     /* may be set to #if 1 by ./configure */

#  define Z_PREFIX_SET

/* Allow a user configurable prefix string, defaulting to "z_" */
#  if (Z_PREFIX == 1)
#    undef Z_PREFIX
#    define Z_PREFIX z_
#  endif
#define ZXDEF(a, b) a ## b
#define ZDEF(a, b) ZXDEF(a,b)

#if 0
/* For debug messages */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#pragma message("define _dist_code: " STR(_dist_code))
#endif

/* all linked symbols and init macros */
#  define _dist_code            ZDEF(Z_PREFIX, _dist_code)
#  define _length_code          ZDEF(Z_PREFIX, _length_code)
#  define _tr_align             ZDEF(Z_PREFIX, _tr_align)
#  define _tr_flush_bits        ZDEF(Z_PREFIX, _tr_flush_bits)
#  define _tr_flush_block       ZDEF(Z_PREFIX, _tr_flush_block)
#  define _tr_init              ZDEF(Z_PREFIX, _tr_init)
#  define _tr_stored_block      ZDEF(Z_PREFIX, _tr_stored_block)
#  define _tr_tally             ZDEF(Z_PREFIX, _tr_tally)
#  define adler32               ZDEF(Z_PREFIX, adler32)
#  define adler32_combine       ZDEF(Z_PREFIX, adler32_combine)
#  define adler32_combine64     ZDEF(Z_PREFIX, adler32_combine64)
#  define adler32_z             ZDEF(Z_PREFIX, adler32_z)
#  ifndef Z_SOLO
#    define compress              ZDEF(Z_PREFIX, compress)
#    define compress2             ZDEF(Z_PREFIX, compress2)
#    define compressBound         ZDEF(Z_PREFIX, compressBound)
#  endif
#  define crc32                 ZDEF(Z_PREFIX, crc32)
#  define crc32_combine         ZDEF(Z_PREFIX, crc32_combine)
#  define crc32_combine64       ZDEF(Z_PREFIX, crc32_combine64)
#  define crc32_z               ZDEF(Z_PREFIX, crc32_z)
#  define deflate               ZDEF(Z_PREFIX, deflate)
#  define deflateBound          ZDEF(Z_PREFIX, deflateBound)
#  define deflateCopy           ZDEF(Z_PREFIX, deflateCopy)
#  define deflateEnd            ZDEF(Z_PREFIX, deflateEnd)
#  define deflateGetDictionary  ZDEF(Z_PREFIX, deflateGetDictionary)
#  define deflateInit           ZDEF(Z_PREFIX, deflateInit)
#  define deflateInit2          ZDEF(Z_PREFIX, deflateInit2)
#  define deflateInit2_         ZDEF(Z_PREFIX, deflateInit2_)
#  define deflateInit_          ZDEF(Z_PREFIX, deflateInit_)
#  define deflateParams         ZDEF(Z_PREFIX, deflateParams)
#  define deflatePending        ZDEF(Z_PREFIX, deflatePending)
#  define deflatePrime          ZDEF(Z_PREFIX, deflatePrime)
#  define deflateReset          ZDEF(Z_PREFIX, deflateReset)
#  define deflateResetKeep      ZDEF(Z_PREFIX, deflateResetKeep)
#  define deflateSetDictionary  ZDEF(Z_PREFIX, deflateSetDictionary)
#  define deflateSetHeader      ZDEF(Z_PREFIX, deflateSetHeader)
#  define deflateTune           ZDEF(Z_PREFIX, deflateTune)
#  define deflate_copyright     ZDEF(Z_PREFIX, deflate_copyright)
#  define get_crc_table         ZDEF(Z_PREFIX, get_crc_table)
#  ifndef Z_SOLO
#    define gz_error              ZDEF(Z_PREFIX, gz_error)
#    define gz_intmax             ZDEF(Z_PREFIX, gz_intmax)
#    define gz_strwinerror        ZDEF(Z_PREFIX, gz_strwinerror)
#    define gzbuffer              ZDEF(Z_PREFIX, gzbuffer)
#    define gzclearerr            ZDEF(Z_PREFIX, gzclearerr)
#    define gzclose               ZDEF(Z_PREFIX, gzclose)
#    define gzclose_r             ZDEF(Z_PREFIX, gzclose_r)
#    define gzclose_w             ZDEF(Z_PREFIX, gzclose_w)
#    define gzdirect              ZDEF(Z_PREFIX, gzdirect)
#    define gzdopen               ZDEF(Z_PREFIX, gzdopen)
#    define gzeof                 ZDEF(Z_PREFIX, gzeof)
#    define gzerror               ZDEF(Z_PREFIX, gzerror)
#    define gzflush               ZDEF(Z_PREFIX, gzflush)
#    define gzfread               ZDEF(Z_PREFIX, gzfread)
#    define gzfwrite              ZDEF(Z_PREFIX, gzfwrite)
#    define gzgetc                ZDEF(Z_PREFIX, gzgetc)
#    define gzgetc_               ZDEF(Z_PREFIX, gzgetc_)
#    define gzgets                ZDEF(Z_PREFIX, gzgets)
#    define gzoffset              ZDEF(Z_PREFIX, gzoffset)
#    define gzoffset64            ZDEF(Z_PREFIX, gzoffset64)
#    define gzopen                ZDEF(Z_PREFIX, gzopen)
#    define gzopen64              ZDEF(Z_PREFIX, gzopen64)
#    ifdef _WIN32
#      define gzopen_w              ZDEF(Z_PREFIX, gzopen_w)
#    endif
#    define gzprintf              ZDEF(Z_PREFIX, gzprintf)
#    define gzputc                ZDEF(Z_PREFIX, gzputc)
#    define gzputs                ZDEF(Z_PREFIX, gzputs)
#    define gzread                ZDEF(Z_PREFIX, gzread)
#    define gzrewind              ZDEF(Z_PREFIX, gzrewind)
#    define gzseek                ZDEF(Z_PREFIX, gzseek)
#    define gzseek64              ZDEF(Z_PREFIX, gzseek64)
#    define gzsetparams           ZDEF(Z_PREFIX, gzsetparams)
#    define gztell                ZDEF(Z_PREFIX, gztell)
#    define gztell64              ZDEF(Z_PREFIX, gztell64)
#    define gzungetc              ZDEF(Z_PREFIX, gzungetc)
#    define gzvprintf             ZDEF(Z_PREFIX, gzvprintf)
#    define gzwrite               ZDEF(Z_PREFIX, gzwrite)
#  endif
#  define inflate               ZDEF(Z_PREFIX, inflate)
#  define inflateBack           ZDEF(Z_PREFIX, inflateBack)
#  define inflateBackEnd        ZDEF(Z_PREFIX, inflateBackEnd)
#  define inflateBackInit       ZDEF(Z_PREFIX, inflateBackInit)
#  define inflateBackInit_      ZDEF(Z_PREFIX, inflateBackInit_)
#  define inflateCodesUsed      ZDEF(Z_PREFIX, inflateCodesUsed)
#  define inflateCopy           ZDEF(Z_PREFIX, inflateCopy)
#  define inflateEnd            ZDEF(Z_PREFIX, inflateEnd)
#  define inflateGetDictionary  ZDEF(Z_PREFIX, inflateGetDictionary)
#  define inflateGetHeader      ZDEF(Z_PREFIX, inflateGetHeader)
#  define inflateInit           ZDEF(Z_PREFIX, inflateInit)
#  define inflateInit2          ZDEF(Z_PREFIX, inflateInit2)
#  define inflateInit2_         ZDEF(Z_PREFIX, inflateInit2_)
#  define inflateInit_          ZDEF(Z_PREFIX, inflateInit_)
#  define inflateMark           ZDEF(Z_PREFIX, inflateMark)
#  define inflatePrime          ZDEF(Z_PREFIX, inflatePrime)
#  define inflateReset          ZDEF(Z_PREFIX, inflateReset)
#  define inflateReset2         ZDEF(Z_PREFIX, inflateReset2)
#  define inflateResetKeep      ZDEF(Z_PREFIX, inflateResetKeep)
#  define inflateSetDictionary  ZDEF(Z_PREFIX, inflateSetDictionary)
#  define inflateSync           ZDEF(Z_PREFIX, inflateSync)
#  define inflateSyncPoint      ZDEF(Z_PREFIX, inflateSyncPoint)
#  define inflateUndermine      ZDEF(Z_PREFIX, inflateUndermine)
#  define inflateValidate       ZDEF(Z_PREFIX, inflateValidate)
#  define inflate_copyright     ZDEF(Z_PREFIX, inflate_copyright)
#  define inflate_fast          ZDEF(Z_PREFIX, inflate_fast)
#  define inflate_table         ZDEF(Z_PREFIX, inflate_table)
#  ifndef Z_SOLO
#    define uncompress            ZDEF(Z_PREFIX, uncompress)
#    define uncompress2           ZDEF(Z_PREFIX, uncompress2)
#  endif
#  define zError                ZDEF(Z_PREFIX, zError)
#  ifndef Z_SOLO
#    define zcalloc               ZDEF(Z_PREFIX, zcalloc)
#    define zcfree                ZDEF(Z_PREFIX, zcfree)
#  endif
#  define zlibCompileFlags      ZDEF(Z_PREFIX, zlibCompileFlags)
#  define zlibVersion           ZDEF(Z_PREFIX, zlibVersion)

/* all zlib typedefs in zlib.h and zconf.h */
#  define Byte                  ZDEF(Z_PREFIX, Byte)
#  define Bytef                 ZDEF(Z_PREFIX, Bytef)
#  define alloc_func            ZDEF(Z_PREFIX, alloc_func)
#  define charf                 ZDEF(Z_PREFIX, charf)
#  define free_func             ZDEF(Z_PREFIX, free_func)
#  ifndef Z_SOLO
#    define gzFile                ZDEF(Z_PREFIX, gzFile)
#  endif
#  define gz_header             ZDEF(Z_PREFIX, gz_header)
#  define gz_headerp            ZDEF(Z_PREFIX, gz_headerp)
#  define in_func               ZDEF(Z_PREFIX, in_func)
#  define intf                  ZDEF(Z_PREFIX, intf)
#  define out_func              ZDEF(Z_PREFIX, out_func)
#  define uInt                  ZDEF(Z_PREFIX, uInt)
#  define uIntf                 ZDEF(Z_PREFIX, uIntf)
#  define uLong                 ZDEF(Z_PREFIX, uLong)
#  define uLongf                ZDEF(Z_PREFIX, uLongf)
#  define voidp                 ZDEF(Z_PREFIX, voidp)
#  define voidpc                ZDEF(Z_PREFIX, voidpc)
#  define voidpf                ZDEF(Z_PREFIX, voidpf)

/* all zlib structs in zlib.h and zconf.h */
#  define gz_header_s           ZDEF(Z_PREFIX, gz_header_s)
#  define internal_state        ZDEF(Z_PREFIX, internal_state)

#endif

#if defined(__MSDOS__) && !defined(MSDOS)
#  define MSDOS
#endif
#if (defined(OS_2) || defined(__OS2__)) && !defined(OS2)
#  define OS2
#endif
#if defined(_WINDOWS) && !defined(WINDOWS)
#  define WINDOWS
#endif
#if defined(_WIN32) || defined(_WIN32_WCE) || defined(__WIN32__)
#  ifndef WIN32
#    define WIN32
#  endif
#endif
#if (defined(MSDOS) || defined(OS2) || defined(WINDOWS)) && !defined(WIN32)
#  if !defined(__GNUC__) && !defined(__FLAT__) && !defined(__386__)
#    ifndef SYS16BIT
#      define SYS16BIT
#    endif
#  endif
#endif

/*
 * Compile with -DMAXSEG_64K if the alloc function cannot allocate more
 * than 64k bytes at a time (needed on systems with 16-bit int).
 */
#ifdef SYS16BIT
#  define MAXSEG_64K
#endif
#ifdef MSDOS
#  define UNALIGNED_OK
#endif

#ifdef __STDC_VERSION__
#  ifndef STDC
#    define STDC
#  endif
#  if __STDC_VERSION__ >= 199901L
#    ifndef STDC99
#      define STDC99
#    endif
#  endif
#endif
#if !defined(STDC) && (defined(__STDC__) || defined(__cplusplus))
#  define STDC
#endif
#if !defined(STDC) && (defined(__GNUC__) || defined(__BORLANDC__))
#  define STDC
#endif
#if !defined(STDC) && (defined(MSDOS) || defined(WINDOWS) || defined(WIN32))
#  define STDC
#endif
#if !defined(STDC) && (defined(OS2) || defined(__HOS_AIX__))
#  define STDC
#endif

#if defined(__OS400__) && !defined(STDC)    /* iSeries (formerly AS/400). */
#  define STDC
#endif

#ifndef STDC
#  ifndef const /* cannot use !defined(STDC) && !defined(const) on Mac */
#    define const       /* note: need a more gentle solution here */
#  endif
#endif

#if defined(ZLIB_CONST) && !defined(z_const)
#  define z_const const
#else
#  define z_const
#endif

#ifdef Z_SOLO
   typedef unsigned long z_size_t;
#else
#  define z_longlong long long
#  if defined(NO_SIZE_T)
     typedef unsigned NO_SIZE_T z_size_t;
#  elif defined(STDC)
#    include <stddef.h>
     typedef size_t z_size_t;
#  else
     typedef unsigned long z_size_t;
#  endif
#  undef z_longlong
#endif

/* Maximum value for memLevel in deflateInit2 */
#ifndef MAX_MEM_LEVEL
#  ifdef MAXSEG_64K
#    define MAX_MEM_LEVEL 8
#  else
#    define MAX_MEM_LEVEL 9
#  endif
#endif

/* Maximum value for windowBits in deflateInit2 and inflateInit2.
 * WARNING: reducing MAX_WBITS makes minigzip unable to extract .gz files
 * created by gzip. (Files created by minigzip can still be extracted by
 * gzip.)
 */
#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif

/* The memory requirements for deflate are (in bytes):
            (1 << (windowBits+2)) +  (1 << (memLevel+9))
 that is: 128K for windowBits=15  +  128K for memLevel = 8  (default values)
 plus a few kilobytes for small objects. For example, if you want to reduce
 the default memory requirements from 256K to 128K, compile with
     make CFLAGS="-O -DMAX_WBITS=14 -DMAX_MEM_LEVEL=7"
 Of course this will generally degrade compression (there's no free lunch).

   The memory requirements for inflate are (in bytes) 1 << windowBits
 that is, 32K for windowBits=15 (default value) plus about 7 kilobytes
 for small objects.
*/

                        /* Type declarations */

#ifndef OF /* function prototypes */
#  ifdef STDC
#    define OF(args)  args
#  else
#    define OF(args)  ()
#  endif
#endif

#ifndef Z_ARG /* function prototypes for stdarg */
#  if defined(STDC) || defined(Z_HAVE_STDARG_H)
#    define Z_ARG(args)  args
#  else
#    define Z_ARG(args)  ()
#  endif
#endif

/* The following definitions for FAR are needed only for MSDOS mixed
 * model programming (small or medium model with some far allocations).
 * This was tested only with MSC; for other MSDOS compilers you may have
 * to define NO_MEMCPY in zutil.h.  If you don't need the mixed model,
 * just define FAR to be empty.
 */
#ifdef SYS16BIT
#  if defined(M_I86SM) || defined(M_I86MM)
     /* MSC small or medium model */
#    define SMALL_MEDIUM
#    ifdef _MSC_VER
#      define FAR _far
#    else
#      define FAR far
#    endif
#  endif
#  if (defined(__SMALL__) || defined(__MEDIUM__))
     /* Turbo C small or medium model */
#    define SMALL_MEDIUM
#    ifdef __BORLANDC__
#      define FAR _far
#    else
#      define FAR far
#    endif
#  endif
#endif

#if defined(WINDOWS) || defined(WIN32)
   /* If building or using zlib as a DLL, define ZLIB_DLL.
    * This is not mandatory, but it offers a little performance increase.
    */
#  ifdef ZLIB_DLL
#    if defined(WIN32) && (!defined(__BORLANDC__) || (__BORLANDC__ >= 0x500))
#      ifdef ZLIB_INTERNAL
#        define ZEXTERN extern __declspec(dllexport)
#      else
#        define ZEXTERN extern __declspec(dllimport)
#      endif
#    endif
#  endif  /* ZLIB_DLL */
   /* If building or using zlib with the WINAPI/WINAPIV calling convention,
    * define ZLIB_WINAPI.
    * Caution: the standard ZLIB1.DLL is NOT compiled using ZLIB_WINAPI.
    */
#  ifdef ZLIB_WINAPI
#    ifdef FAR
#      undef FAR
#    endif
#    include <windows.h>
     /* No need for _export, use ZLIB.DEF instead. */
     /* For complete Windows compatibility, use WINAPI, not __stdcall. */
#    define ZEXPORT WINAPI
#    ifdef WIN32
#      define ZEXPORTVA WINAPIV
#    else
#      define ZEXPORTVA FAR CDECL
#    endif
#  endif
#endif

#if defined (__BEOS__)
#  ifdef ZLIB_DLL
#    ifdef ZLIB_INTERNAL
#      define ZEXPORT   __declspec(dllexport)
#      define ZEXPORTVA __declspec(dllexport)
#    else
#      define ZEXPORT   __declspec(dllimport)
#      define ZEXPORTVA __declspec(dllimport)
#    endif
#  endif
#endif

#ifndef ZEXTERN
#  define ZEXTERN extern
#endif
#ifndef ZEXPORT
#  define ZEXPORT
#endif
#ifndef ZEXPORTVA
#  define ZEXPORTVA
#endif

#ifndef FAR
#  define FAR
#endif

#if !defined(__MACTYPES__)
typedef unsigned char  Byte;  /* 8 bits */
#endif
typedef unsigned int   uInt;  /* 16 bits or more */
typedef unsigned long  uLong; /* 32 bits or more */

#ifdef SMALL_MEDIUM
   /* Borland C/C++ and some old MSC versions ignore FAR inside typedef */
#  define Bytef Byte FAR
#else
   typedef Byte  FAR Bytef;
#endif
typedef char  FAR charf;
typedef int   FAR intf;
typedef uInt  FAR uIntf;
typedef uLong FAR uLongf;

#ifdef STDC
   typedef void const *voidpc;
   typedef void FAR   *voidpf;
   typedef void       *voidp;
#else
   typedef Byte const *voidpc;
   typedef Byte FAR   *voidpf;
   typedef Byte       *voidp;
#endif

#if !defined(Z_U4) && !defined(Z_SOLO) && defined(STDC)
#  include <limits.h>
#  if (UINT_MAX == 0xffffffffUL)
#    define Z_U4 unsigned
#  elif (ULONG_MAX == 0xffffffffUL)
#    define Z_U4 unsigned long
#  elif (USHRT_MAX == 0xffffffffUL)
#    define Z_U4 unsigned short
#  endif
#endif

#ifdef Z_U4
   typedef Z_U4 z_crc_t;
#else
   typedef unsigned long z_crc_t;
#endif

#ifdef HAVE_UNISTD_H    /* may be set to #if 1 by ./configure */
#  define Z_HAVE_UNISTD_H
#endif

#ifdef HAVE_STDARG_H    /* may be set to #if 1 by ./configure */
#  define Z_HAVE_STDARG_H
#endif

#ifdef STDC
#  ifndef Z_SOLO
#    include <sys/types.h>      /* for off_t */
#  endif
#endif

#if defined(STDC) || defined(Z_HAVE_STDARG_H)
#  ifndef Z_SOLO
#    include <stdarg.h>         /* for va_list */
#  endif
#endif

#ifdef _WIN32
#  ifndef Z_SOLO
#    include <stddef.h>         /* for wchar_t */
#  endif
#endif

/* a little trick to accommodate both "#define _LARGEFILE64_SOURCE" and
 * "#define _LARGEFILE64_SOURCE 1" as requesting 64-bit operations, (even
 * though the former does not conform to the LFS document), but considering
 * both "#undef _LARGEFILE64_SOURCE" and "#define _LARGEFILE64_SOURCE 0" as
 * equivalently requesting no 64-bit operations
 */
#if defined(_LARGEFILE64_SOURCE) && -_LARGEFILE64_SOURCE - -1 == 1
#  undef _LARGEFILE64_SOURCE
#endif

#if defined(__WATCOMC__) && !defined(Z_HAVE_UNISTD_H)
#  define Z_HAVE_UNISTD_H
#endif
#ifndef Z_SOLO
#  if defined(Z_HAVE_UNISTD_H) || defined(_LARGEFILE64_SOURCE)
#    include <unistd.h>         /* for SEEK_*, off_t, and _LFS64_LARGEFILE */
#    ifdef VMS
#      include <unixio.h>       /* for off_t */
#    endif
#    ifndef z_off_t
#      define z_off_t off_t
#    endif
#  endif
#endif

#if defined(_LFS64_LARGEFILE) && _LFS64_LARGEFILE-0
#  define Z_LFS64
#endif

#if defined(_LARGEFILE64_SOURCE) && defined(Z_LFS64)
#  define Z_LARGE64
#endif

#if defined(_FILE_OFFSET_BITS) && _FILE_OFFSET_BITS-0 == 64 && defined(Z_LFS64)
#  define Z_WANT64
#endif

#if !defined(SEEK_SET) && !defined(Z_SOLO)
#  define SEEK_SET        0       /* Seek from beginning of file.  */
#  define SEEK_CUR        1       /* Seek from current position.  */
#  define SEEK_END        2       /* Set file pointer to EOF plus "offset" */
#endif

#ifndef z_off_t
#  define z_off_t long
#endif

#if !defined(_WIN32) && defined(Z_LARGE64)
#  define z_off64_t off64_t
#else
#  if defined(_WIN32) && !defined(__GNUC__) && !defined(Z_SOLO)
#    define z_off64_t __int64
#  else
#    define z_off64_t z_off_t
#  endif
#endif

/* MVS linker does not support external names larger than 8 bytes */
#if defined(__MVS__)
  #pragma map(deflateInit_,"DEIN")
  #pragma map(deflateInit2_,"DEIN2")
  #pragma map(deflateEnd,"DEEND")
  #pragma map(deflateBound,"DEBND")
  #pragma map(inflateInit_,"ININ")
  #pragma map(inflateInit2_,"ININ2")
  #pragma map(inflateEnd,"INEND")
  #pragma map(inflateSync,"INSY")
  #pragma map(inflateSetDictionary,"INSEDI")
  #pragma map(compressBound,"CMBND")
  #pragma map(inflate_table,"INTABL")
  #pragma map(inflate_fast,"INFA")
  #pragma map(inflate_copyright,"INCOPY")
#endif

#endif /* ZCONF_H */
