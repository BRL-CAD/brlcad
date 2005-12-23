/* src/bltConfig.h.  Generated automatically by configure.  */
/* src/bltConfig.h.in.  Generated automatically from configure.in by autoheader.  */

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#undef HAVE_SYS_WAIT_H

/* Define to `int' if <sys/types.h> doesn't define.  */
#if defined(_MSC_VER) || defined(__BORLANDC__)
#define pid_t int
#endif /* _MSC_VER || __BORLANDC__ */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
#undef size_t

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS	1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#undef TIME_WITH_SYS_TIME

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
#undef WORDS_BIGENDIAN	


/* Define if DBL_EPSILON is not defined in float.h */
#undef BLT_DBL_EPSILON

/* Define if drand48 isn't declared in math.h. */
#define NEED_DECL_DRAND48	1

/* Define if srand48 isn't declared in math.h. */
#define NEED_DECL_SRAND48	1

/* Define if strdup isn't declared in a standard header file. */
#undef NEED_DECL_STRDUP

/* Define if j1 isn't declared in a standard header file. */
#define NEED_DECL_J1		1

/* Define if union wait type is defined incorrectly.  */
#undef HAVE_UNION_WAIT

/* Define if isfinite is found in libm.  */
#undef HAVE_ISFINITE

/* The number of bytes in a long.  */
#define SIZEOF_LONG		4

/* The number of bytes in a long long.  */
#define SIZEOF_LONG_LONG	8

/* The number of bytes in a void *.  */
#define SIZEOF_VOID_P		4

/* Define if you have the XExtendedMaxRequestSize function.  */
#undef HAVE_XEXTENDEDMAXREQUESTSIZE

/* Define if you have the drand48 function.  */
#define HAVE_DRAND48		1

/* Define if you have the finite function.  */
#undef HAVE_FINITE

/* Define if you have the srand48 function.  */
#define HAVE_SRAND48		1

/* Define if you have the strdup function.  */
#define HAVE_STRDUP		1

#ifndef __BORLANDC__
/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP		1

/* Define if you have the strncasecmp function.  */
#define HAVE_STRNCASECMP		1
#endif

/* Define if you have the <ctype.h> header file.  */
#define HAVE_CTYPE_H		1

/* Define if you have the <errno.h> header file.  */
#define HAVE_ERRNO_H		1

/* Define if you have the <float.h> header file.  */
#define HAVE_FLOAT_H		1

/* Define if you have the <ieeefp.h> header file.  */
#undef HAVE_IEEEFP_H

/* Define if you have the <jpeglib.h> header file.  */
/* Defined in Makefile */
/* #undef HAVE_JPEGLIB_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H		1

/* Define if you have the <malloc.h> header file.  */
#define HAVE_MALLOC_H		1

/* Define if you have the <math.h> header file.  */
#define HAVE_MATH_H		1

/* Define if you have the <memory.h> header file.  */
#define HAVE_MEMORY_H		1

/* Define if you have the <setjmp.h> header file.  */
#define HAVE_SETJMP_H		1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H		1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H		1

/* Define if you have the <sys/param.h> header file.  */
#undef HAVE_SYS_PARAM_H

/* Define if you have the <sys/time.h> header file.  */
#undef HAVE_SYS_TIME_H		

/* Define if you have the <sys/wait.h> header file.  */
#undef HAVE_SYS_WAIT_H

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the <waitflags.h> header file.  */
#undef HAVE_WAITFLAGS_H

/* Define if you have the m library (-lm).  */
#define HAVE_LIBM		1

/* Define if you have the nsl library (-lnsl).  */
#undef HAVE_LIBNSL

/* Define if you have the socket library (-lsocket).  */
#undef HAVE_LIBSOCKET

