/*                          C O N F I G _ W I N . H
 * BRL-CAD
 *
 * Copyright (C) 1993-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file config_win.h
 *
 */
#ifndef CONFIG_H
#define CONFIG_H seen

#if defined(WIN32)
/* XXX - This is temporary (atleast until a brlcad_config.h is
 * auto-generated on windows)
 */
#define __STDC__ 1

#pragma warning( disable : 4244 4305 4018)
/*  4244 conversion from type 1 to type 2 
    4305 truncation
	4018 signed/unsigned mismatch
*/
/* Microsoft VisualC++ 6.0 on WinNT 4.0 */
/*
 * Ensure that Project Settings / Project Options includes
 *	/Za		for ANSI C
 *	/D "WIN32"	to fire this rule
 */
# if !__STDC__
#	error "STDC is not properly set on WIN32 build, add /Za to Project Settings / Project Options"
# endif

#define BRLCAD_ROOT ""
#define INSTALL_DIRECTORY "C:/brlcad7_0"

#define HAS_OPENGL	1
#define HAVE_CALLOC	1
#define HAVE_ERRNO_H	1
#define HAVE_FCNTL_H	1
#define HAVE_FLOAT_H	1
#define HAVE_GETHOSTNAME	1
#define HAVE_GL_GL_H	1
#define HAVE_IO_H	1
#define HAVE_LIMITS_H	1
#define HAVE_MATHERR	1
#define HAVE_MEMORY_H	1
#define HAVE_OFF_T	1
#define HAVE_PWD_H	1
#define HAVE_REGEX_H	1
#define HAVE_STDARG_H	1
#define HAVE_STDLIB_H	1
#define HAVE_STRCHR	1
#define HAVE_STRCHR	1
#define HAVE_STRDUP	1
#define HAVE_STRDUP_DECL	1
#define HAVE_STRING_H	1
#define HAVE_SYS_TIME	1
#define HAVE_TIME	1
#define HAVE_TIME_H	1
#define HAVE_VARARGS_H	1
#define HAVE_VFORK	1
#define HAVE_VPRINTF	1
#define REVERSE_IEEE	yes
#define USE_PROTOTYPES	1

/* XXX - do not rely on config_win.h providing these headers.  they
 * will be removed at some point.
 */
#include <windows.h>
#include <io.h>

#define bzero(str,n)		memset( str, 0, n )
#define bcopy(from,to,count)	memcpy( to, from, count )

#define	isnan _isnan
#define MAXPATHLEN _MAX_PATH
#define O_CREAT _O_CREAT
#define O_EXCL _O_EXCL
#define O_RDONLY _O_RDONLY
#define O_RDWR _O_RDWR
#define access _access
#define chmod _chmod
#define close _close
#define commit _commit
#define creat _creat
#define dup _dup
#define dup2 _dup2
#define eof _eof
#define fdopen _fdopen
#define fileno _fileno
#define fstat _fstat
#define getpid _getpid
#define hypot _hypot
#define isascii __isascii
#define isatty _isatty
#define locking _locking
#define lseek _lseek
#define mktemp _mktemp
#define off_t _off_t
#define open _open
#define pclose _pclose
#define pipe _pipe
#define popen _popen
#define read _read
#define setmode _setmode
#define sopen _sopen
#define stat _stat
#define strdup _strdup
#define tell _tell
#define umask _umask
#define unlink _unlink
#define write _write
#undef DELETE
#undef IN
#undef OUT
#undef complex
#undef rad1
#undef rad2

#endif /* if defined(WIN32) */

#endif /* CONFIG_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
