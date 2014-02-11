/*                      D E F I N E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @defgroup container Data Containers */
/**   @defgroup avs Attribute/Value Sets */
/**   @defgroup bitv Bit Vectors */
/**   @defgroup color Color */
/**   @defgroup hash Hash Tables */
/**   @defgroup list Linked Lists */
/**   @defgroup parse Structure Parsing */
/**   @defgroup ptbl Pointer Tables */
/**   @defgroup rb Red-Black Trees */
/**   @defgroup vlb Variable-length Byte Buffers */
/**   @defgroup vls Variable-length Strings */
/** @defgroup memory Memory Management */
/**   @defgroup magic Magic Numbers */
/**   @defgroup malloc Allocation & Deallocation */
/**   @defgroup mf Memory-mapped Files */
/** @defgroup io Input/Output */
/**   @defgroup log Logging */
/**   @defgroup debug Debugging */
/**   @defgroup file File Processing */
/**   @defgroup vfont Vector Fonts */
/** @defgroup data Data Management */
/**   @defgroup cmd Command History */
/**   @defgroup conv Data Conversion */
/**   @defgroup getopt Command-line Option Parsing*/
/**   @defgroup hton Network Byte-order Conversion */
/**   @defgroup hist Histogram Handling */
/** @defgroup parallel  Parallel Processing */
/**   @defgroup thread Multithreading */
/** @defgroup binding Language Bindings */
/**   @defgroup tcl Tcl Interfacing */

/** @file defines.h
 *
 * Commonly used definitions for the BRL-CAD Utility Library, LIBBU.
 *
 * The two letters "BU" stand for "BRL-CAD" and "Utility".  This
 * library provides several layers of low-level utility routines,
 * providing features that make cross-platform coding easier.
 *
 * Parallel processing support:  threads, semaphores, parallel-malloc.
 * Consolidated logging support:  bu_log(), bu_exit(), and bu_bomb().
 *
 * The intention is that these routines are general extensions to the
 * data types offered by the C language itself, and to the basic C
 * runtime support provided by the system LIBC.  All routines in LIBBU
 * are designed to be "parallel-safe" (sometimes called "mp-safe" or
 * "thread-safe" if parallelism is via threading) to greatly ease code
 * development for multiprocessor systems.
 *
 * All of the data types provided by this library are defined in bu.h;
 * none of the routines in this library will depend on data types
 * defined in other BRL-CAD header files, such as vmath.h.  Look for
 * those routines in LIBBN.
 *
 * All truly fatal errors detected by the library use bu_bomb() to
 * exit with a status of 12.  The LIBBU variants of system calls
 * (e.g., bu_malloc()) do not return to the caller (unless there's a
 * bomb hook defined) unless they succeed, thus sparing the programmer
 * from constantly having to check for NULL return codes.
 *
 */
#ifndef BU_DEFINES_H
#define BU_DEFINES_H

#ifndef BU_EXPORT
#  if defined(BU_DLL_EXPORTS) && defined(BU_DLL_IMPORTS)
#    error "Only BU_DLL_EXPORTS or BU_DLL_IMPORTS can be defined, not both."
#  elif defined(BU_DLL_EXPORTS)
#    define BU_EXPORT __declspec(dllexport)
#  elif defined(BU_DLL_IMPORTS)
#    define BU_EXPORT __declspec(dllimport)
#  else
#    define BU_EXPORT
#  endif
#endif

#endif  /* BU_DEFINES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
