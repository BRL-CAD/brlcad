/*                            B U . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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
/** @addtogroup libbu */
/** @{ */
/** @file bu.h
 *
 * @brief
 * Header file for the BRL-CAD Utility Library, LIBBU.
 *
 * This library provides several layers of low-level utility routines,
 * providing features that make coding much easier.
 *
 * Parallel processing support:  threads, sempahores, parallel-malloc.
 * Consolidated logging support:  bu_log(), bu_exit(), and bu_bomb().
 *
 * The intention is that these routines are general extensions to the
 * data types offered by the C language itself, and to the basic C
 * runtime support provided by the system LIBC.
 *
 * All of the data types provided by this library are defined in bu.h;
 * none of the routines in this library will depend on data types
 * defined in other BRL-CAD header files, such as vmath.h.  Look for
 * those routines in LIBBN.
 *
 */
#ifndef __BU_H__
#define __BU_H__

#include "common.h"

#include <stdlib.h>
#include <sys/types.h>

__BEGIN_DECLS

#ifndef BU_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef BU_EXPORT_DLL
#      define BU_EXPORT __declspec(dllexport)
#    else
#      define BU_EXPORT __declspec(dllimport)
#    endif
#  else
#    define BU_EXPORT
#  endif
#endif

/* NOTE: do not rely on these values */
#define BRLCAD_OK 0
#define BRLCAD_ERROR 1

/**
 * Flag for non-case-sensitive searching
 */
#define BU_CASEFOLD 0x10


/**
 * @def BU_DIR_SEPARATOR
 * the default directory separator character
 */
#ifdef DIR_SEPARATOR
#  define BU_DIR_SEPARATOR DIR_SEPARATOR
#else
#  ifdef DIR_SEPARATOR_2
#    define BU_DIR_SEPARATOR DIR_SEPARATOR_2
#  else
#    ifdef _WIN32
#      define BU_DIR_SEPARATOR '\\'
#    else
#      define BU_DIR_SEPARATOR '/'
#    endif  /* _WIN32 */
#  endif  /* DIR_SEPARATOR_2 */
#endif  /* DIR_SEPARATOR */

/**
 * Maximum length of a filesystem path.  Typically defined in a system
 * file but if it isn't set, we create it.
 */
#ifndef MAXPATHLEN
#  ifdef _MAX_PATH
#    define MAXPATHLEN _MAX_PATH
#  else
#    define MAXPATHLEN 1024
#  endif
#endif

/**
 * set to the path list separator character
 */
#if defined(PATH_SEPARATOR)
#  define BU_PATH_SEPARATOR PATH_SEPARATOR
#else
#  if defined(_WIN32)
#    define BU_PATH_SEPARATOR ';'
#  else
#    define BU_PATH_SEPARATOR ':'
#  endif  /* _WIN32 */
#endif  /* PATH_SEPARATOR */


/**
 * @def BU_FLSTR
 *
 * Macro for getting a concatenated string of the current file and
 * line number.  Produces something of the form: "filename.c"":""1234"
 */
#define bu_cpp_str(s) # s
#define bu_cpp_xstr(s)  bu_cpp_str(s)
#define bu_cpp_glue(a, b) a ## b
#define bu_cpp_xglue(a, b) bu_cpp_glue(a, b)
#define BU_FLSTR __FILE__ ":" bu_cpp_xstr(__LINE__)

/*
 * Macros for providing function prototypes, regardless of whether
 * the compiler understands them or not.  It is vital that the
 * argument list given for "args" be enclosed in parens.
 */
#define BU_EXTERN(type_and_name, args) extern type_and_name args
#define BU_ARGS(args) args

/**
 * shorthand declaration of a printf-style functions
 */
#define __BU_ATTR_FORMAT12 __attribute__ ((__format__ (__printf__, 1, 2)))
#define __BU_ATTR_FORMAT23 __attribute__ ((__format__ (__printf__, 2, 3)))

/**
 * shorthand declaration of a function that doesn't return
 */
#define __BU_ATTR_NORETURN __attribute__ ((__noreturn__))

/**
 *  If we're compiling strict, turn off "format string vs arguments"
 *  checks - BRL-CAD customizes the arguments to some of these
 *  function types (adding bu_vls support) and that is a problem with
 *  strict checking.
 */
#if defined(STRICT_FLAGS)
#  undef __BU_ATTR_FORMAT12
#  undef __BU_ATTR_FORMAT23
#  undef __BU_ATTR_NORETURN
#  define __BU_ATTR_FORMAT12
#  define __BU_ATTR_FORMAT23
#  define __BU_ATTR_NORETURN
#endif


/*
 * I N T E R F A C E H E A D E R S
 */

/* system interface headers */
#include <setjmp.h> /* for bu_setjmp */
#include <stddef.h> /* for size_t */
#include <limits.h> /* for CHAR_BIT */

#ifdef HAVE_STDINT_H
#  include <stdint.h> /* for [u]int[16|32|64]_t */
#endif

/* common interface headers */
#include "tcl.h"	/* Included for Tcl_Interp definition */
#include "magic.h"

/* FIXME Temporary global interp.  Remove me.  */
BU_EXPORT extern Tcl_Interp *brlcad_interp;

/**
 * B U _ F O R T R A N
 *
 * @def BU_FORTRAN
 *
 * This macro is used to take the 'C' function name, and convert it at
 * compile time to the FORTRAN calling convention.
 *
 * Lower case, with a trailing underscore.
 */
#define BU_FORTRAN(lc, uc)	lc ## _


/**
 * Handy memory allocator macro
 *
 * @def BU_GETSTRUCT(ptr, struct_type)
 * Acquire storage for a given struct_type.
 * e.g., BU_GETSTRUCT(ptr, structname);
 *
 * @def BU_GETUNION(ptr, union_type)
 * Allocate storage for a union
 */
#define BU_GETSTRUCT(_p, _str) \
	_p = (struct _str *)bu_calloc(1, sizeof(struct _str), #_str " (getstruct)" BU_FLSTR)
#define BU_GETUNION(_p, _unn) \
	_p = (union _unn *)bu_calloc(1, sizeof(union _unn), #_unn " (getunion)" BU_FLSTR)


/**
 * B U _ G E T T Y P E
 *
 * Acquire storage for a given TYPE, eg, BU_GETTYPE(ptr, typename);
 * Equivalent to BU_GETSTRUCT, except without the 'struct' Useful
 * for typedef'ed objects.
 */
#define BU_GETTYPE(_p, _type) \
	_p = (_type *)bu_calloc(1, sizeof(_type), #_type " (gettype)")


/**
 * B U _ A S S E R T
 *
 * @def BU_ASSERT(eqn)
 * Quick and easy macros to generate an informative error message and
 * abort execution if the specified condition does not hold true.
 *
 * @def BU_ASSERT_PTR(eqn)
 * Quick and easy macros to generate an informative error message and
 * abort execution if the specified condition does not hold true.
 *
 * @def BU_ASSERT_LONG(eqn)
 * Quick and easy macros to generate an informative error message and
 * abort execution if the specified condition does not hold true.
 *
 * @def BU_ASSERT_DOUBLE(eqn)
 * Quick and easy macros to generate an informative error message and
 * abort execution if the specified condition does not hold true.
 *
 * Example: BU_ASSERT_LONG(j+7, <, 42);
 */
#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT(_equation)
#else
#  define BU_ASSERT(_equation)	\
	if (!(_equation)) { \
		bu_log("BU_ASSERT(" #_equation ") failed, file %s, line %d\n", \
			__FILE__, __LINE__); \
		bu_bomb("BU_ASSERT failure\n"); \
	}
#endif

#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_PTR(_lhs, _relation, _rhs)
#else
#  define BU_ASSERT_PTR(_lhs, _relation, _rhs)	\
	if (!((_lhs) _relation (_rhs))) { \
		bu_log("BU_ASSERT_PTR(" #_lhs #_relation #_rhs ") failed, lhs=%p, rhs=%p, file %s, line %d\n", \
			(void *)(_lhs), (void *)(_rhs), \
			__FILE__, __LINE__); \
		bu_bomb("BU_ASSERT_PTR failure\n"); \
	}
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_LONG(_lhs, _relation, _rhs)
#else
#  define BU_ASSERT_LONG(_lhs, _relation, _rhs)	\
	if (!((_lhs) _relation (_rhs))) { \
		bu_log("BU_ASSERT_LONG(" #_lhs #_relation #_rhs ") failed, lhs=%ld, rhs=%ld, file %s, line %d\n", \
			(long)(_lhs), (long)(_rhs), \
			__FILE__, __LINE__); \
		bu_bomb("BU_ASSERT_LONG failure\n"); \
	}
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_SIZE_T(_lhs, _relation, _rhs)
#else
#  define BU_ASSERT_SIZE_T(_lhs, _relation, _rhs)	\
	if (!((_lhs) _relation (_rhs))) { \
		bu_log("BU_ASSERT_SIZE_T(" #_lhs #_relation #_rhs ") failed, lhs=%zd, rhs=%zd, file %s, line %d\n", \
			(size_t)(_lhs), (size_t)(_rhs), \
			__FILE__, __LINE__); \
		bu_bomb("BU_ASSERT_SIZE_T failure\n"); \
	}
#endif


#ifdef NO_BOMBING_MACROS
#  define BU_ASSERT_DOUBLE(_lhs, _relation, _rhs)
#else
#  define BU_ASSERT_DOUBLE(_lhs, _relation, _rhs)	\
	if (!((_lhs) _relation (_rhs))) { \
		bu_log("BU_ASSERT_DOUBLE(" #_lhs #_relation #_rhs ") failed, lhs=%lf, rhs=%lf, file %s, line %d\n", \
			(double)(_lhs), (double)(_rhs), \
			__FILE__, __LINE__); \
		bu_bomb("BU_ASSERT_DOUBLE failure\n"); \
	}
#endif
/** @} */

/** @file libbu/vers.c
 *
 * version information about LIBBU
 *
 */

/**
 * returns the compile-time version of libbu
 */
BU_EXPORT BU_EXTERN(const char *bu_version, (void));


/**
 * genptr_t - A portable way of declaring a "generic" pointer that is
 * wide enough to point to anything, which can be used on both ANSI C
 * and K&R C environments.  On some machines, pointers to functions
 * can be wider than pointers to data bytes, so a declaration of
 * "char*" isn't generic enough.
 *
 * DEPRECATED: use void* instead
 */
#if !defined(GENPTR_NULL)
typedef void *genptr_t;
#  define GENPTR_NULL ((genptr_t)0)
#endif


/**
 * MAX_PSW - The maximum number of processors that can be expected on
 * this hardware.  Used to allocate application-specific per-processor
 * tables at compile-time and represent a hard limit on the number of
 * processors/threads that may be spawned. The actual number of
 * available processors is found at runtime by calling rt_avail_cpus()
 */
#define MAX_PSW 1024


/*----------------------------------------------------------------------*/
/** @addtogroup hton */
/** @{ */
/**
 * Sizes of "network" format data.  We use the same convention as the
 * TCP/IP specification, namely, big-Endian, IEEE format, twos
 * compliment.  This is the BRL-CAD external data representation
 * (XDR).  See also the support routines in libbu/xdr.c
 *
 */
#define SIZEOF_NETWORK_SHORT  2	/* htons(), bu_gshort(), bu_pshort() */
#define SIZEOF_NETWORK_LONG   4	/* htonl(), bu_glong(), bu_plong() */
#define SIZEOF_NETWORK_FLOAT  4	/* htonf() */
#define SIZEOF_NETWORK_DOUBLE 8	/* htond() */
/** @} */


/*----------------------------------------------------------------------*/
/** @addtogroup conv */
/** @{*/
/** @file convert.c
 *
 * @brief
 * Routines to translate data formats.  The data formats are:
 *
 * \li Host/Network		is the data in host format or local format
 * \li  signed/unsigned		Is the data signed?
 * \li char/short/int/long/double
 *				Is the data 8bits, 16bits, 32bits, 64bits
 *				or a double?
 *
 * The method of conversion is to convert up to double then back down the
 * the expected output format.
 *
 */

/**
 * b u _ c v _ c o o k i e
 *
 * @brief
 * Set's a bit vector after parsing an input string.
 *
 * Set up the conversion tables/flags for vert.
 *
 * @param in	format description.
 *
 * @return a 32 bit vector.
 *
 * Format description:
 * [channels][h|n][s|u] c|s|i|l|d|8|16|32|64 [N|C|L]
 *
 * @n channels must be null or 1
 * @n Host | Network
 * @n signed | unsigned
 * @n char | short | integer | long | double | number of bits of integer
 * @n Normalize | Clip | low-order
 */
BU_EXPORT BU_EXTERN(int bu_cv_cookie, (const char *in));

/**
 * C V _ O P T I M I Z E
 *
 * It is always more efficient to handle host data, rather than
 * network.  If host and network formats are the same, and the request
 * was for network format, modify the cookie to request host format.
 */
BU_EXPORT BU_EXTERN(int bu_cv_optimize, (int cookie));

/**
 * C V _ I T E M L E N
 *
 * Returns the number of bytes each "item" of type "cookie" occupies.
 */
BU_EXPORT BU_EXTERN(int bu_cv_itemlen, (int cookie));

/**
 * bu_cv_w_cookie
 *
 * @brief
 * convert with cookie
 *
 * @param in		input pointer
 * @param incookie	input format cookie.
 * @param count		number of entries to convert.
 * @param out		output pointer.
 * @param outcookie	output format cookie.
 * @param size		size of output buffer in bytes;
 *
 *
 * A worst case would be:	ns16 on vax to ns32
 @code
 *	ns16 	-> hs16
 *		-> hd
 *		-> hs32
 *		-> ns32
 @endcode
 * The worst case is probably the easiest to deal with because all
 * steps are done.  The more difficult cases are when only a subset of
 * steps need to be done.
 *
 * @par Method:
 @code
 *	HOSTDBL defined as true or false
 *	if ! hostother then
 *		hostother = (Endian == END_BIG) ? SAME : DIFFERENT;
 *	fi
 *	if (infmt == double) then
 *		if (HOSTDBL == SAME) {
 *			inIsHost = host;
 *		fi
 *	else
 *		if (hostother == SAME) {
 *			inIsHost = host;
 *		fi
 *	fi
 *	if (outfmt == double) then
 *		if (HOSTDBL == SAME) {
 *			outIsHost == host;
 *	else
 *		if (hostother == SAME) {
 *			outIsHost = host;
 *		fi
 *	fi
 *	if (infmt == outfmt) {
 *		if (inIsHost == outIsHost) {
 *			copy(in, out)
 *			exit
 *		else if (inIsHost == net) {
 *			ntoh?(in, out);
 *			exit
 *		else
 *			hton?(in, out);
 *			exit
 *		fi
 *	fi
 *
 *	while not done {
 *		from = in;
 *
 *		if (inIsHost == net) {
 *			ntoh?(from, t1);
 *			from = t1;
 *		fi
 *		if (infmt != double) {
 *			if (outIsHost == host) {
 *				to = out;
 *			else
 *				to = t2;
 *			fi
 *			castdbl(from, to);
 *			from = to;
 *		fi
 *
 *		if (outfmt == double) {
 *			if (outIsHost == net) {
 *				hton?(from, out);
 *			fi
 *		else
 *			if (outIsHost == host) {
 *				dblcast(from, out);
 *			else
 *				dblcast(from, t3);
 *				hton?(t3, out);
 *			fi
 *		fi
 *	done
 @endcode
*/
BU_EXPORT BU_EXTERN(size_t bu_cv_w_cookie, (genptr_t, int, size_t, genptr_t, int, size_t));

/**
 * bu_cv_ntohss
 *
 * @brief
 * Network TO Host Signed Short
 *
 * It is assumed that this routine will only be called if there is
 * real work to do.  Ntohs does no checking to see if it is reasonable
 * to do any conversions.
 *
 *
 * @param in	generic pointer for input.
 * @param count	number of shorts to be generated.
 * @param out	short pointer for output
 * @param size	number of bytes of space reserved for out.
 *
 *
 * @return	number of conversions done.
 */
BU_EXPORT BU_EXTERN(size_t bu_cv_ntohss,
		    (signed short *,
		     size_t,
		     genptr_t,
		     size_t));
BU_EXPORT BU_EXTERN(size_t bu_cv_ntohus,
		    (unsigned short *,
		     size_t,
		     genptr_t,
		     size_t));
BU_EXPORT BU_EXTERN(size_t bu_cv_ntohsl,
		    (signed long int *,
		     size_t,
		     genptr_t,
		     size_t));
BU_EXPORT BU_EXTERN(size_t bu_cv_ntohul,
		    (unsigned long int *,
		     size_t,
		     genptr_t,
		     size_t));
BU_EXPORT BU_EXTERN(size_t bu_cv_htonss,
		    (genptr_t,
		     size_t,
		     signed short *,
		     size_t));
BU_EXPORT BU_EXTERN(size_t bu_cv_htonus,
		    (genptr_t,
		     size_t,
		     unsigned short *,
		     size_t));
BU_EXPORT BU_EXTERN(size_t bu_cv_htonsl,
		    (genptr_t,
		     size_t,
		     long *,
		     size_t));
BU_EXPORT BU_EXTERN(size_t bu_cv_htonul,
		    (genptr_t,
		     size_t,
		     unsigned long *,
		     size_t));

#define CV_CHANNEL_MASK 0x00ff
#define CV_HOST_MASK    0x0100
#define CV_SIGNED_MASK  0x0200
#define CV_TYPE_MASK    0x1c00  /* 0001 1100 0000 0000 */
#define CV_CONVERT_MASK 0x6000  /* 0110 0000 0000 0000 */

#define CV_TYPE_SHIFT    10
#define CV_CONVERT_SHIFT 13

#define CV_8  0x0400
#define CV_16 0x0800
#define CV_32 0x0c00
#define CV_64 0x1000
#define CV_D  0x1400

#define CV_CLIP   0x0000
#define CV_NORMAL 0x2000
#define CV_LIT    0x4000

/** deprecated */
#define END_NOTSET 0
#define END_BIG    1	/* PowerPC/MIPS */
#define END_LITTLE 2	/* Intel */
#define END_ILL    3	/* PDP-11 */
#define END_CRAY   4	/* Old Cray */

/** deprecated */
#define IND_NOTSET 0
#define IND_BIG    1
#define IND_LITTLE 2
#define IND_ILL    3
#define IND_CRAY   4


/*----------------------------------------------------------------------*/
/** @file endian.c
 *
 * Run-time byte order detection.
 *
 */

typedef enum {
    BU_LITTLE_ENDIAN = 1234, /* LSB first: i386, VAX order */
    BU_BIG_ENDIAN    = 4321, /* MSB first: 68000, IBM, network order */
    BU_PDP_ENDIAN    = 3412  /* LSB first in word, MSW first in long */
} bu_endian_t;


/**
 * b u _ b y t e o r d e r
 *
 * returns the platform byte ordering (e.g., big-/little-endian)
 */
BU_EXPORT BU_EXTERN(bu_endian_t bu_byteorder, (void));


/**@}*/

/*----------------------------------------------------------------------*/

/** @addtogroup bu_list */
/** @{ */
/** @file list.c
 *
 * @brief Support routines for doubly-linked lists.
 *
 * These macros assume that all user-provided structures will have a
 * "struct bu_list" as their first element (often named "l" [ell]).
 * Thus, a pointer to the bu_list struct is a "pun" for the
 * user-provided structure as well, and the pointers can be converted
 * back and forth safely with type casts.
 *
 * Furthermore, the head of the linked list could be a full instance
 * of the user-provided structure (although the storage-conscious
 * programmer could make the head just an bu_list structure, with
 * careful type casting).  This results in a doubly-linked circular
 * list, with the head having the same shape as all the list members.
 * The application is free to make use of this symmetry and store data
 * values in the head, or the extra storage in the head can be
 * ignored.
 *
 * Where a macro expects an argument "p", it should be a pointer to a
 * user-provided structure.
 *
 * Where a macro expects an argument "hp", it should be a pointer to a
 * "struct bu_list" located in the list head, e.g., &(head.l).
 *
 * Where a macro expects an argument "old", "new", or "cur", it should
 * be a pointer to the "struct bu_list" located either in a
 * user-provided structure, e.g. &((p)->l), or for the case of "old"
 * it may also be in the list head.
 *
 * --- BEGIN EXAMPLE ---
 * @code

 // make bu_list the first element in your structure
 struct my_structure {
 struct bu_list l;
 int my_data;
 };

 // your actual list
 struct my_structure *my_list = NULL;

 // allocate and initialize your list head
 BU_GETSTRUCT(my_list, my_structure);
 BU_LIST_INIT(&(my_list->l));
 my_list->my_data = -1;

 // add a new element to your list
 struct my_structure *new_entry;
 BU_GETSTRUCT(new_entry, my_structure);
 new_entry->my_data = rand();
 BU_LIST_PUSH(&(my_list->l), &(new_entry->l));

 // iterate over your list, remove all items
 struct my_structure *entry;
 while (BU_LIST_WHILE(entry, my_structure, &(my_list->l))) {
 bu_log("Entry value is %d\n", entry->my_data);
 BU_LIST_DEQUEUE(&(entry->l));
 bu_free(entry, "free my_structure entry");
 }
 bu_free(my_list, "free my_structure list head");

 * @endcode
 * --- END EXAMPLE ---
 *
 * Dequeueing the head of a list is a valid and well defined operation
 * which should be performed with caution.  Unless a pointer to some
 * other element of the list is retained by the application, the rest
 * of the linked list can no longer be referred to.
 *
 * The "magic" field of the list header _must_ be set to the constant
 * BU_LIST_HEAD_MAGIC, but the "magic" field of all list members
 * should be established by user code, to identify the type of
 * structure that the bu_list structure is embedded in.  It is
 * permissible for one list to contain an arbitrarily mixed set of
 * user "magic" numbers, as long as the head is properly marked.
 *
 * There is a dual set of terminology used in some of the macros:
 *
 * FIRST/ LAST - from the point of view of the list head
 * NEXT / PREV - from the point of view of a list member
 * forw / back - the actual pointer names
 */

struct bu_list {
    unsigned long magic;		/**< @brief Magic # for mem id/check */
    struct bu_list *forw;		/**< @brief "forward", "next" */
    struct bu_list *back;		/**< @brief "back", "last" */
};
#define BU_LIST_NULL ((struct bu_list *)0)

typedef struct bu_list bu_list_t;

#define BU_LIST_CLOSE(hp) { \
	BU_ASSERT((hp) != NULL); \
	if ((hp) == NULL) \
		return; \
	BU_ASSERT(BU_LIST_IS_EMPTY((hp))); \
	bu_list_free((hp)); \
	bu_free((char *)(hp), "bu_list head"); \
}


/**
 * Insert "new" item in front of "old" item.  Often, "old" is the
 * head.  To put the new item at the tail of the list, insert before
 * the head, e.g.  * BU_LIST_INSERT(&(head.l), &((p)->l));
 */
#define BU_LIST_INSERT(old, new) { \
	(new)->back = (old)->back; \
	(old)->back = (new); \
	(new)->forw = (old); \
	(new)->back->forw = (new);  }

/**
 * Append "new" item after "old" item.  Often, "old" is the head.  To
 * put the new item at the head of the list, append after the head,
 * e.g.  * BU_LIST_APPEND(&(head.l), &((p)->l));
 */
#define BU_LIST_APPEND(old, new) { \
	(new)->forw = (old)->forw; \
	(new)->back = (old); \
	(old)->forw = (new); \
	(new)->forw->back = (new);  }

/**
 * Dequeue "cur" item from anywhere in doubly-linked list
 */
#define BU_LIST_DEQUEUE(cur) { \
	(cur)->forw->back = (cur)->back; \
	(cur)->back->forw = (cur)->forw; \
	(cur)->forw = (cur)->back = BU_LIST_NULL;  /* sanity */ }

/**
 * Dequeue "cur" but do not fix its links
 */
#define BU_LIST_DQ(cur) {\
	(cur)->forw->back = (cur)->back; \
	(cur)->back->forw = (cur)->forw; }

#define BU_LIST_DQ_T(cur, type) (\
	(cur)->forw->back = (cur)->back, \
	(cur)->back->forw = (cur)->forw, \
	(type *)(cur))

/**
 * This version of BU_LIST_DEQUEUE uses the comma operator inorder to
 * return a typecast version of the dequeued pointer
 */
#define BU_LIST_DEQUEUE_T(cur, type) (\
	(cur)->forw->back = (cur)->back, \
	(cur)->back->forw = (cur)->forw, \
	(cur)->forw = (cur)->back = BU_LIST_NULL, \
	(type *)(cur))


/**
 * The Stack Discipline
 *
 * BU_LIST_PUSH places p at the tail of hp.  BU_LIST_POP sets p to
 * last element in hp's list (else NULL) and, if p is non-null,
 * dequeues it.
 */
#define BU_LIST_PUSH(hp, p)					\
	BU_LIST_APPEND(hp, (struct bu_list *)(p))

#define BU_LIST_POP(structure, hp, p)				\
	{							\
		if (BU_LIST_NON_EMPTY(hp))				\
		{							\
		    (p) = ((struct structure *)((hp)->forw));		\
		    BU_LIST_DEQUEUE((struct bu_list *)(p));		\
		}							\
		else							\
		     (p) = (struct structure *) 0;			\
	}

#define BU_LIST_POP_T(hp, type)				\
	(type *)bu_list_pop(hp)

/**
 * "Bulk transfer" all elements from the list headed by src_hd onto
 * the list headed by dest_hd, without examining every element in the
 * list.  src_hd is left with a valid but empty list.
 *
 * BU_LIST_INSERT_LIST places src_hd elements at head of dest_hd list,
 * BU_LIST_APPEND_LIST places src_hd elements at end of dest_hd list.
 */
#define BU_LIST_INSERT_LIST(dest_hp, src_hp) \
	if (BU_LIST_NON_EMPTY(src_hp)) { \
		struct bu_list *_first = (src_hp)->forw; \
		struct bu_list *_last = (src_hp)->back; \
		(dest_hp)->forw->back = _last; \
		_last->forw = (dest_hp)->forw; \
		(dest_hp)->forw = _first; \
		_first->back = (dest_hp); \
		(src_hp)->forw = (src_hp)->back = (src_hp); \
	}

#define BU_LIST_APPEND_LIST(dest_hp, src_hp) \
	if (BU_LIST_NON_EMPTY(src_hp)) {\
		struct bu_list *_first = (src_hp)->forw; \
		struct bu_list *_last = (src_hp)->back; \
		_first->back = (dest_hp)->back; \
		(dest_hp)->back->forw = _first; \
		(dest_hp)->back = _last; \
		_last->forw = (dest_hp); \
		(src_hp)->forw = (src_hp)->back = (src_hp); \
	}

/**
 * Test if a doubly linked list is empty, given head pointer
 */
#define BU_LIST_IS_EMPTY(hp)	((hp)->forw == (hp))
#define BU_LIST_NON_EMPTY(hp)	((hp)->forw != (hp))
#define BU_LIST_NON_EMPTY_P(p, structure, hp)	\
	(((p)=(struct structure *)((hp)->forw)) != (struct structure *)(hp))
#define BU_LIST_IS_CLEAR(hp)	((hp)->magic == 0 && \
			(hp)->forw == BU_LIST_NULL && \
			(hp)->back == BU_LIST_NULL)

/* Handle list initialization */
#define BU_LIST_UNINITIALIZED(hp)	((hp)->forw == BU_LIST_NULL)
#define BU_LIST_IS_INITIALIZED(hp)	((hp)->forw != BU_LIST_NULL)
#define BU_LIST_INIT(hp) { \
	(hp)->forw = (hp)->back = (hp); \
	(hp)->magic = BU_LIST_HEAD_MAGIC;	/* used by circ. macros */ }
#define BU_LIST_MAGIC_SET(hp, val) {(hp)->magic = (val);}
#define BU_LIST_MAGIC_OK(hp, val)	((hp)->magic == (val))
#define BU_LIST_MAGIC_WRONG(hp, val)	((hp)->magic != (val))

/* Return re-cast pointer to first element on list.
 * No checking is performed to see if list is empty.
 */
#define BU_LIST_LAST(structure, hp)	\
	((struct structure *)((hp)->back))
#define BU_LIST_BACK(structure, hp)	\
	((struct structure *)((hp)->back))
#define BU_LIST_PREV(structure, hp)	\
	((struct structure *)((hp)->back))
#define BU_LIST_FIRST(structure, hp)	\
	((struct structure *)((hp)->forw))
#define BU_LIST_FORW(structure, hp)	\
	((struct structure *)((hp)->forw))
#define BU_LIST_NEXT(structure, hp)	\
	((struct structure *)((hp)->forw))

/**
 * Boolean test to see if current list element is the head
 */
#define BU_LIST_IS_HEAD(p, hp)	\
	(((struct bu_list *)(p)) == (hp))
#define BU_LIST_NOT_HEAD(p, hp)	\
	(((struct bu_list *)(p)) != (hp))
#define BU_CK_LIST_HEAD(_p)	BU_CKMAG((_p), BU_LIST_HEAD_MAGIC, "bu_list")

/**
 * Boolean test to see if previous list element is the head
 */
#define BU_LIST_PREV_IS_HEAD(p, hp)\
	(((struct bu_list *)(p))->back == (hp))
#define BU_LIST_PREV_NOT_HEAD(p, hp)\
	(((struct bu_list *)(p))->back != (hp))

/**
 * Boolean test to see if the next list element is the head
 */
#define BU_LIST_NEXT_IS_HEAD(p, hp)	\
	(((struct bu_list *)(p))->forw == (hp))
#define BU_LIST_NEXT_NOT_HEAD(p, hp)	\
	(((struct bu_list *)(p))->forw != (hp))

#define BU_LIST_EACH(hp, p, type) \
	 for ((p)=(type *)BU_LIST_FIRST(bu_list, hp); \
	      (p) && BU_LIST_NOT_HEAD(p, hp); \
	      (p)=(type *)BU_LIST_PNEXT(bu_list, p)) \

#define BU_LIST_REVEACH(hp, p, type) \
	 for ((p)=(type *)BU_LIST_LAST(bu_list, hp); \
	      (p) && BU_LIST_NOT_HEAD(p, hp); \
	      (p)=(type *)BU_LIST_PREV(bu_list, ((struct bu_list *)(p)))) \

#define BU_LIST_TAIL(hp, start, p, type) \
	 for ((p)=(type *)start; \
	      (p) && BU_LIST_NOT_HEAD(p, hp); \
	      (p)=(type *)BU_LIST_PNEXT(bu_list, (p)))

/**
 * Intended as innards for a for loop to visit all nodes on list, e.g.:
 *
 * for (BU_LIST_FOR(p, structure, hp)) {
 *	work_on(p);
 * }
 */
#define BU_LIST_FOR(p, structure, hp)	\
	(p)=BU_LIST_FIRST(structure, hp); \
	(p) && BU_LIST_NOT_HEAD(p, hp); \
	(p)=BU_LIST_PNEXT(structure, p)

#define BU_LIST_FOR_BACKWARDS(p, structure, hp)	\
	(p)=BU_LIST_LAST(structure, hp); \
	(p) && BU_LIST_NOT_HEAD(p, hp); \
	(p)=BU_LIST_PLAST(structure, p)

/**
 * Process all the list members except hp and the actual head.  Useful
 * when starting somewhere besides the head.
 */
#define BU_LIST_FOR_CIRC(p, structure, hp)	\
	(p)=BU_LIST_PNEXT_CIRC(structure, hp); \
	(p) && (p) != (hp); \
	(p)=BU_LIST_PNEXT_CIRC(structure, p)

/**
 * Intended as innards for a for loop to visit elements of two lists
 * in tandem, e.g.:
 *
 * for (BU_LIST_FOR2(p1, p2, structure, hp1, hp2)) {
 *	process(p1, p2);
 * }
 */
#define BU_LIST_FOR2(p1, p2, structure, hp1, hp2)				\
		(p1)=BU_LIST_FIRST(structure, hp1),			\
		(p2)=BU_LIST_FIRST(structure, hp2);			\
		(p1) && BU_LIST_NOT_HEAD((struct bu_list *)(p1), (hp1)) &&	\
		(p2) && BU_LIST_NOT_HEAD((struct bu_list *)(p2), (hp2));		\
		(p1)=BU_LIST_NEXT(structure, (struct bu_list *)(p1)),	\
		(p2)=BU_LIST_NEXT(structure, (struct bu_list *)(p2))

/**
 * Innards for a while loop that constantly picks off the first
 * element.  Useful mostly for a loop that will dequeue every list
 * element, e.g.:
 *
 *	while (BU_LIST_WHILE(p, structure, hp)) {
 *@n		BU_LIST_DEQUEUE(&(p->l));
 *@n		free((char *)p);
 *@n	}
 */
#define BU_LIST_WHILE(p, structure, hp)	\
	(((p)=(struct structure *)((hp)->forw)) != (struct structure *)(hp))

/**
 * Return the magic number of the first (or last) item on a list
 */
#define BU_LIST_FIRST_MAGIC(hp)		((hp)->forw->magic)
#define BU_LIST_LAST_MAGIC(hp)		((hp)->back->magic)

/**
 * Return pointer to next (or previous) element, which may be the head
 */
#define BU_LIST_PNEXT(structure, p)	\
	((struct structure *)(((struct bu_list *)(p))->forw))
#define BU_LIST_PLAST(structure, p)	\
	((struct structure *)(((struct bu_list *)(p))->back))

/**
 * Return pointer two links away, which may include the head
 */
#define BU_LIST_PNEXT_PNEXT(structure, p)	\
	((struct structure *)(((struct bu_list *)(p))->forw->forw))
#define BU_LIST_PNEXT_PLAST(structure, p)	\
	((struct structure *)(((struct bu_list *)(p))->forw->back))
#define BU_LIST_PLAST_PNEXT(structure, p)	\
	((struct structure *)(((struct bu_list *)(p))->back->forw))
#define BU_LIST_PLAST_PLAST(structure, p)	\
	((struct structure *)(((struct bu_list *)(p))->back->back))

/**
 * Return pointer to circular next element; ie, ignoring the list head
 */
#define BU_LIST_PNEXT_CIRC(structure, p)	\
	((BU_LIST_FIRST_MAGIC((struct bu_list *)(p)) == BU_LIST_HEAD_MAGIC) ? \
		BU_LIST_PNEXT_PNEXT(structure, (struct bu_list *)(p)) : \
		BU_LIST_PNEXT(structure, p))

/**
 * Return pointer to circular last element; ie, ignoring the list head
 */
#define BU_LIST_PPREV_CIRC(structure, p)	\
	((BU_LIST_LAST_MAGIC((struct bu_list *)(p)) == BU_LIST_HEAD_MAGIC) ? \
		BU_LIST_PLAST_PLAST(structure, (struct bu_list *)(p)) : \
		BU_LIST_PLAST(structure, p))

/**
 * Support for membership on multiple linked lists.
 *
 * When a structure of type '_type' contains more than one bu_list
 * structure within it (such as the NMG edgeuse), this macro can be
 * used to convert a pointer '_ptr2' to a "midway" bu_list structure
 * (an element called '_name2' in structure '_type') back into a
 * pointer to the overall enclosing structure.  Examples:
 *
 * eu = BU_LIST_MAIN_PTR(edgeuse, midway, l2);
 *
 * eu1 = BU_LIST_MAIN_PTR(edgeuse, BU_LIST_FIRST(bu_list, &eg1->eu_hd2), l2);
 *
 * Files using BU_LIST_MAIN_PTR will need to include stddef.h
 */
#define BU_LIST_MAIN_PTR(_type, _ptr2, _name2)	\
	((struct _type *)(((char *)(_ptr2)) - offsetof(struct _type, _name2.magic)))
/** @} */


/**
 * fastf_t - Intended to be the fastest floating point data type on
 * the current machine, with at least 64 bits of precision.  On 16 and
 * 32 bit machine, this is typically "double", but on 64 bit machines,
 * it is often "float".  Virtually all floating point variables (and
 * more complicated data types, like vect_t and mat_t) are defined as
 * fastf_t.  The one exception is when a subroutine return is a
 * floating point value; that is always declared as "double".
 *
 * TODO: If used pervasively, it should eventually be possible to make
 * fastf_t a GMP C++ type for fixed-precision computations.
 */
typedef double fastf_t;

/**
 * Definitions about limits of floating point representation
 * Eventually, should be tied to type of hardware (IEEE, IBM, Cray)
 * used to implement the fastf_t type.
 *
 * MAX_FASTF - Very close to the largest value that can be held by a
 * fastf_t without overflow.  Typically specified as an integer power
 * of ten, to make the value easy to spot when printed.  TODO: macro
 * function syntax instead of constant (DEPRECATED)
 *
 * SQRT_MAX_FASTF - sqrt(MAX_FASTF), or slightly smaller.  Any number
 * larger than this, if squared, can be expected to * produce an
 * overflow.  TODO: macro function syntax instead of constant
 * (DEPRECATED)
 *
 * SMALL_FASTF - Very close to the smallest value that can be
 * represented while still being greater than zero.  Any number
 * smaller than this (and non-negative) can be considered to be
 * zero; dividing by such a number can be expected to produce a
 * divide-by-zero error.  All divisors should be checked against
 * this value before actual division is performed.  TODO: macro
 * function sytax instead of constant (DEPRECATED)
 *
 * SQRT_SMALL_FASTF - sqrt(SMALL_FASTF), or slightly larger.  The
 * value of this is quite a lot larger than that of SMALL_FASTF.  Any
 * number smaller than this, when squared, can be expected to produce
 * a zero result.  TODO: macro function syntax instead of constant
 * (DEPRECATED)
 *
 */
#if defined(vax) || (defined(sgi) && !defined(mips))
/* DEC VAX "D" format, the most restrictive */
#  define MAX_FASTF		1.0e37	/* Very close to the largest number */
#  define SQRT_MAX_FASTF	1.0e18	/* This squared just avoids overflow */
#  define SMALL_FASTF		1.0e-37	/* Anything smaller is zero */
#  define SQRT_SMALL_FASTF	1.0e-18	/* This squared gives zero */
#else
/* IBM format, being the next most restrictive format */
#  define MAX_FASTF		1.0e73	/* Very close to the largest number */
#  define SQRT_MAX_FASTF	1.0e36	/* This squared just avoids overflow */
#  define SMALL_FASTF		1.0e-77	/* Anything smaller is zero */
#  if defined(aux)
#    define SQRT_SMALL_FASTF	1.0e-40 /* _doprnt error in libc */
#  else
#    define SQRT_SMALL_FASTF	1.0e-39	/* This squared gives zero */
#  endif
#endif

/** DEPRECATED, do not use */
#define SMALL SQRT_SMALL_FASTF



/*----------------------------------------------------------------------*/
/** @addtogroup bitv */
/** @{*/
/** @file bitv.c
 *
 * Routines for managing efficient high-performance bit vectors of
 * arbitrary length.
 *
 * The basic type "bitv_t" is defined in include/bu.h; it is the
 * widest integer datatype for which efficient hardware support
 * exists.  BU_BITV_SHIFT and BU_BITV_MASK are also defined in bu.h
 *
 * These bit vectors are "little endian", bit 0 is in the right hand
 * side of the [0] word.
 *
 */

/**
 * bitv_t should be a fast integer type for implementing bit vectors.
 *
 * On many machines, this is a 32-bit "long", but on some machines a
 * compiler/vendor-specific type such as "long long" or even 'char'
 * can give access to faster integers.
 *
 * THE SIZE OF bitv_t MUST MATCH BU_BITV_SHIFT.
 */
typedef unsigned char bitv_t;

/**
 * Bit vector shift size
 *
 * Should equal to: log2(sizeof(bitv_t)*8.0).  Using bu_bitv_shift()
 * will return a run-time computed shift size if the size of a bitv_t
 * changes.  Performance impact is rather minimal for most models but
 * disabled for a handful of primitives that heavily rely on bit
 * vectors.
 *
 * (8-bit type: 3, 16-bit type: 4, 32-bit type: 5, 64-bit type: 6)
 */
#ifdef CHAR_BIT
#  if CHAR_BIT == 8
#    define BU_BITV_SHIFT 3
#  elif CHAR_BIT == 16
#    define BU_BITV_SHIFT 4
#  elif CHAR_BIT == 32
#    define BU_BITV_SHIFT 5
#  elif CHAR_BIT == 64
#    define BU_BITV_SHIFT 6
#  endif
#else
#  define BU_BITV_SHIFT bu_bitv_shift()
#endif

/** Bit vector mask */
#define BU_BITV_MASK ((1<<BU_BITV_SHIFT)-1)

/**
 * @brief
 * Bit vector data structure.
 *
 * bu_bitv uses a little-endian encoding, placing bit 0 on the right
 * side of the 0th word.
 *
 * This is done only because left-shifting a 1 can be done in an
 * efficient word-length-independent manner; going the other way would
 * require a compile-time constant with only the sign bit set, and an
 * unsigned right shift, which some machines don't have in hardware,
 * or an extra subtraction.
 *
 * Application code should *never* peak at the bit-buffer; use the
 * macros.  The external hex form is most signigicant byte first (bit
 * 0 is at the right).  Note that MUVES does it differently.
 */
struct bu_bitv {
    struct bu_list l;		/**< @brief linked list for caller's use */
    unsigned int nbits;		/**< @brief actual size of bits[], in bits */
    bitv_t bits[2];	/**< @brief variable size array */
};

#define BU_CK_BITV(_vp)		BU_CKMAG(_vp, BU_BITV_MAGIC, "bu_bitv")

/**
 * b u _ b i t v _ s h i f t
 *
 * returns floor(log2(sizeof(bitv_t)*8.0)), i.e. the number of bits
 * required with base-2 encoding to index any bit in an array of
 * length sizeof(bitv_t)*8.0 bits long.  users should not call this
 * directly, instead calling the BU_BITV_SHIFT macro instead.
 */
BU_EXPORT BU_EXTERN(unsigned int bu_bitv_shift, ());

/*
 * Bit-string manipulators for arbitrarily long bit strings stored as
 * an array of bitv_t's.
 */
#define BU_BITS2BYTES(_nb)	(BU_BITS2WORDS(_nb)*sizeof(bitv_t))
#define BU_BITS2WORDS(_nb)	(((_nb)+BU_BITV_MASK)>>BU_BITV_SHIFT)
#define BU_WORDS2BITS(_nw)	((_nw)*sizeof(bitv_t)*8)



#if 1
#define BU_BITTEST(_bv, bit)	\
	(((_bv)->bits[(bit)>>BU_BITV_SHIFT] & (((bitv_t)1)<<((bit)&BU_BITV_MASK)))!=0)
#else
static __inline__ int BU_BITTEST(volatile void * addr, int nr)
{
    int oldbit;

    __asm__ __volatile__(
	"btl %2, %1\n\tsbbl %0, %0"
	:"=r" (oldbit)
	:"m" (addr), "Ir" (nr));
    return oldbit;
}
#endif

#define BU_BITSET(_bv, bit)	\
	((_bv)->bits[(bit)>>BU_BITV_SHIFT] |= (((bitv_t)1)<<((bit)&BU_BITV_MASK)))
#define BU_BITCLR(_bv, bit)	\
	((_bv)->bits[(bit)>>BU_BITV_SHIFT] &= ~(((bitv_t)1)<<((bit)&BU_BITV_MASK)))

/**
 * zeros all of the internal storage bytes in a bit vector array
 */
#define BU_BITV_ZEROALL(_bv)	\
{ \
	if ((_bv) && (_bv)->nbits != 0) { \
		unsigned char *bvp = (unsigned char *)(_bv)->bits; \
		size_t nbytes = BU_BITS2BYTES((_bv)->nbits); \
		do { \
			*bvp++ = (unsigned char)0; \
		} while (--nbytes != 0); \
	} \
}


/* This is not done by default for performance reasons */
#ifdef NO_BOMBING_MACROS
#  define BU_BITV_BITNUM_CHECK(_bv, _bit)
#else
#  define BU_BITV_BITNUM_CHECK(_bv, _bit)	/* Validate bit number */ \
	if (((unsigned)(_bit)) >= (_bv)->nbits) {\
		bu_log("BU_BITV_BITNUM_CHECK bit number (%u) out of range (0..%u)\n", \
			((unsigned)(_bit)), (_bv)->nbits); \
		bu_bomb("process self-terminating\n");\
	}
#endif

#ifdef NO_BOMBING_MACROS
#  define BU_BITV_NBITS_CHECK(_bv, _nbits)
#else
#  define BU_BITV_NBITS_CHECK(_bv, _nbits)	/* Validate number of bits */ \
	if (((unsigned)(_nbits)) > (_bv)->nbits) {\
		bu_log("BU_BITV_NBITS_CHECK number of bits (%u) out of range (> %u)", \
			((unsigned)(_nbits)), (_bv)->nbits); \
		bu_bomb("process self-terminating"); \
	}
#endif


/**
 * Macros to efficiently find all the ONE bits in a bit vector.
 * Counts words down, counts bits in words going up, for speed &
 * portability.  It does not matter if the shift causes the sign bit
 * to smear to the right.
 *
 * @par Example:
 @code

 BU_BITV_LOOP_START(bv) {
 fiddle(BU_BITV_LOOP_INDEX);
 } BU_BITV_LOOP_END;

 @endcode
 *
 */
#define BU_BITV_LOOP_START(_bv)	\
{ \
	int _wd;	/* Current word number */  \
	BU_CK_BITV(_bv); \
	for (_wd=BU_BITS2WORDS((_bv)->nbits)-1; _wd>=0; _wd--) {  \
		int _b;	/* Current bit-in-word number */  \
		bitv_t _val;	/* Current word value */  \
		if ((_val = (_bv)->bits[_wd])==0) continue;  \
		for (_b=0; _b < BU_BITV_MASK+1; _b++, _val >>= 1) { \
			if (!(_val & 1))  continue;

/**
 * This macro is valid only between a BU_BITV_LOOP_START/LOOP_END
 * pair, and gives the bit number of the current iteration.
 */
#define BU_BITV_LOOP_INDEX ((_wd << BU_BITV_SHIFT) | _b)

/**
 * Paired with BU_BITV_LOOP_START()
 */
#define BU_BITV_LOOP_END	\
		} /* end for (_b) */ \
	} /* end for (_wd) */ \
} /* end block */
/** @} */

/*----------------------------------------------------------------------*/
/** @addtogroup bu_hist */
/** @{ */
/** @file hist.c
 *
 * @brief
 * General purpose histogram handling routines.
 *
 * The macro RT_HISTOGRAM_TALLY is used to record items that
 * live in a single "bin", while the subroutine rt_hist_range()
 * is used to record items that may extend across multiple "bin"s.
 *
 */

/**
 * B U _ H I S T
 *
 * @struct bu_hist bu.h
 * @brief histogram support
 */
struct bu_hist  {
    unsigned long magic;	/**< @brief magic # for id/check */
    fastf_t hg_min;		/**< @brief minimum value */
    fastf_t hg_max;		/**< @brief maximum value */
    fastf_t hg_clumpsize;	/**< @brief (max-min+1)/nbins+1 */
    long hg_nsamples;		/**< @brief total number of samples spread into histogram */
    long hg_nbins;		/**< @brief # of bins in hg_bins[]  */
    long *hg_bins;		/**< @brief array of counters */
};
#define BU_CK_HIST(_p)	BU_CKMAG(_p, BU_HIST_MAGIC, "struct bu_hist")

#define BU_HIST_TALLY(_hp, _val) { \
	if ((_val) <= (_hp)->hg_min) { \
		(_hp)->hg_bins[0]++; \
	} else if ((_val) >= (_hp)->hg_max) { \
		(_hp)->hg_bins[(_hp)->hg_nbins]++; \
	} else { \
		(_hp)->hg_bins[(int)(((_val)-(_hp)->hg_min)/(_hp)->hg_clumpsize)]++; \
	} \
	(_hp)->hg_nsamples++;  }

#define BU_HIST_TALLY_MULTIPLE(_hp, _val, _count) { \
	int __count = (_count); \
	if ((_val) <= (_hp)->hg_min) { \
		(_hp)->hg_bins[0] += __count; \
	} else if ((_val) >= (_hp)->hg_max) { \
		(_hp)->hg_bins[(_hp)->hg_nbins] += __count; \
	} else { \
		(_hp)->hg_bins[(int)(((_val)-(_hp)->hg_min)/(_hp)->hg_clumpsize)] += __count; \
	} \
	(_hp)->hg_nsamples += __count;  }

/** @} */
/*----------------------------------------------------------------------*/
/* ptbl.c */
/** @addtogroup ptbl */
/** @{ */
/** @file ptbl.c
 *
 * @brief Support for generalized "pointer tables"
 *
 * Support for generalized "pointer tables", kept compactly in a
 * dynamic array.
 *
 * The table is currently un-ordered, and is merely a array of
 * pointers.  The support routine nmg_tbl manipulates the array for
 * you.  Pointers to be operated on (inserted, deleted, searched for)
 * are passed as a "pointer to long".
 *
 */


/**
 * @struct bu_ptbl
 * @brief
 * Support for generalized "pointer tables".
 */
struct bu_ptbl {
    struct bu_list l; /**< @brief linked list for caller's use */
    off_t end; /**< @brief index into buffer of first available location */
    size_t blen; /**< @brief # of (long *)'s worth of storage at *buffer */
    long **buffer; /**< @brief data storage area */
};
#define BU_CK_PTBL(_p)		BU_CKMAG(_p, BU_PTBL_MAGIC, "bu_ptbl")

#define BU_PTBL_INIT	0	/**< @brief initialize table pointer struct & get storage */
#define BU_PTBL_INS	1	/**< @brief insert an item (long *) into a table */
#define BU_PTBL_LOC 	2	/**< @brief locate a (long *) in an existing table */
#define BU_PTBL_FREE	3	/**< @brief deallocate buffer associated with a table */
#define BU_PTBL_RST	4	/**< @brief empty a table, but keep storage on hand */
#define BU_PTBL_CAT	5	/**< @brief catenate one table onto another */
#define BU_PTBL_RM	6	/**< @brief remove all occurrences of an item from a table */
#define BU_PTBL_INS_UNIQUE 7	/**< @brief insert item into table, if not present */
#define BU_PTBL_ZERO	8	/**< @brief replace all occurrences of an item by 0 */
/*
 * For those routines that have to "peek" into the ptbl a little bit.
 */
#define BU_PTBL_BASEADDR(ptbl)	((ptbl)->buffer)
#define BU_PTBL_LASTADDR(ptbl)	((ptbl)->buffer + (ptbl)->end - 1)
#define BU_PTBL_END(ptbl)	((ptbl)->end)
#define BU_PTBL_LEN(p)	((p)->end)
#define BU_PTBL_GET(ptbl, i)	((ptbl)->buffer[(i)])
#define BU_PTBL_SET(ptbl, i, val)	((ptbl)->buffer[(i)] = (long*)(val))
#define BU_PTBL_TEST(ptbl)	((ptbl)->l.magic == BU_PTBL_MAGIC)
#define BU_PTBL_CLEAR_I(_ptbl, _i) ((_ptbl)->buffer[(_i)] = (long *)0)

/**
 * A handy way to visit all the elements of the table is:
 *
 * struct edgeuse **eup;
 * for (eup = (struct edgeuse **)BU_PTBL_LASTADDR(&eutab); eup >= (struct edgeuse **)BU_PTBL_BASEADDR(&eutab); eup--) {
 *     NMG_CK_EDGEUSE(*eup);
 * }
 * --- OR ---
 * for (BU_PTBL_FOR(eup, (struct edgeuse **), &eutab)) {
 *     NMG_CK_EDGEUSE(*eup);
 * }
 */
#define BU_PTBL_FOR(ip, cast, ptbl)	\
    ip = cast BU_PTBL_LASTADDR(ptbl); ip >= cast BU_PTBL_BASEADDR(ptbl); ip--


/* vlist, vlblock?  But they use vmath.h .. hrm. */
/** @} */

/*----------------------------------------------------------------------*/
/** @addtogroup mf */
/** @{ */
/** @file mappedfile.c
 *
 * @brief
 * Routines for sharing large read-only data files.
 *
 * Routines for sharing large read-only data files like height fields,
 * bit map solids, texture maps, etc.  Uses memory mapped files where
 * available.
 *
 * Each instance of the file has the raw data available as element
 * "buf".  If a particular application needs to transform the raw data
 * in a manner that is identical across all uses of that application
 * (e.g. height fields, EBMs, etc), then the application should
 * provide a non-null "appl" string, to tag the format of the "apbuf".
 * This will keep different applications from sharing that instance of
 * the file.
 *
 * Thus, if the same filename is opened for interpretation as both an
 * EBM and a height field, they will be assigned different mapped file
 * structures, so that the "apbuf" pointers are distinct.
 *
 */

/**
 * @struct bu_mapped_file bu.h
 *
 * @brief
 * Structure for opening a mapped file.
 *
 * Each file is opened and mapped only once (per application, as
 * tagged by the string in "appl" field).  Subsequent opens require an
 * exact match on both strings.
 *
 * Before allocating apbuf and performing data conversion into it,
 * openers should check to see if the file has already been opened and
 * converted previously.
 *
 * When used in RT, the mapped files are not closed at the end of a
 * frame, so that subsequent frames may take advantage of the large
 * data files having already been read and converted.  Examples
 * include EBMs, texture maps, and height fields.
 *
 * For appl == "db_i", file is a ".g" database & apbuf is (struct db_i *).
 */
struct bu_mapped_file {
    struct bu_list l;
    char *name;		/**< @brief bu_strdup() of file name */
    genptr_t buf;	/**< @brief In-memory copy of file (may be mmapped)  */
    size_t buflen;	/**< @brief # bytes in 'buf'  */
    int is_mapped;	/**< @brief 1=mmap() used, 0=bu_malloc/fread */
    char *appl;		/**< @brief bu_strdup() of tag for application using 'apbuf'  */
    genptr_t apbuf;	/**< @brief opt: application-specific buffer */
    size_t apbuflen;	/**< @brief opt: application-specific buflen */
    long modtime;	/**< @brief date stamp, in case file is modified */
    int uses;		/**< @brief # ptrs to this struct handed out */
    int dont_restat;	/**< @brief 1=on subsequent opens, don't re-stat()  */
};
#define BU_CK_MAPPED_FILE(_p)	BU_CKMAG(_p, BU_MAPPED_FILE_MAGIC, "bu_mapped_file")

/** @} */
/*----------------------------------------------------------------------*/

/* formerly rt_g.rtg_logindent, now use bu_log_indent_delta() */
typedef int (*bu_hook_t)BU_ARGS((genptr_t, genptr_t));

struct bu_hook_list {
    struct bu_list l; /**< @brief linked list */
    bu_hook_t hookfunc; /**< @brief function to call */
    genptr_t clientdata; /**< @brief data for caller */
};

#define BU_HOOK_NULL 0
#define BU_HOOK_LIST_NULL ((struct bu_hook_list *) 0)

/** list of callbacks to call during bu_bomb, used by mged. */
BU_EXPORT extern struct bu_hook_list bu_bomb_hook_list;

/*----------------------------------------------------------------------*/
/** @addtogroup avs */
/** @{ */
/** @file avs.c
 *
 * @brief
 * Routines to manage attribute/value sets.
 *
 */

/**
 * B U _ A T T R I B U T E _ V A L U E _ P A I R
 *
 * These strings may or may not be individually allocated, it depends
 * on usage.
 */
struct bu_attribute_value_pair {
    const char *name;	/**< @brief attribute name */
    const char *value; /**< @brief attribute value */
};

/**
 * B U _ A T T R I B U T E _ V A L U E _ S E T
 *
 * A variable-sized attribute-value-pair array.
 *
 * avp points to an array of [max] slots.  The interface routines will
 * realloc to extend as needed.
 *
 * In general, each of the names and values is a local copy made with
 * bu_strdup(), and each string needs to be freed individually.
 * However, if a name or value pointer is between readonly_min and
 * readonly_max, then it is part of a big malloc block that is being
 * freed by the caller, and should not be individually freed.
 */
struct bu_attribute_value_set {
    unsigned long magic;
    unsigned int count;	/**< @brief # valid entries in avp */
    unsigned int max;	/**< @brief # allocated slots in avp */
    genptr_t readonly_min;
    genptr_t readonly_max;
    struct bu_attribute_value_pair *avp;	/**< @brief array[max]  */
};
#define BU_CK_AVS(_avp)		BU_CKMAG(_avp, BU_AVS_MAGIC, "bu_attribute_value_set")

#define BU_AVS_FOR(_pp, _avp)	\
	(_pp) = &(_avp)->avp[(_avp)->count-1]; (_pp) >= (_avp)->avp; (_pp)--

/**
 * Some (but not all) attribute name and value string pointers are
 * taken from an on-disk format bu_external block, while others have
 * been bu_strdup()ed and need to be freed.  This macro indicates
 * whether the pointer needs to be freed or not.
 */
#define AVS_IS_FREEABLE(_avsp, _p)	\
	((_avsp)->readonly_max == NULL || \
	    ((genptr_t)(_p) < (genptr_t)(_avsp)->readonly_min || (genptr_t)(_p) > (genptr_t)(_avsp)->readonly_max))

/** @} */

/*----------------------------------------------------------------------*/
/** @addtogroup vls */
/** @{ */
/** @file vls.c
 *
 @brief
 * Variable Length Strings
 *
 * Assumption:  libc-provided sprintf() function is safe to use in parallel,
 * on parallel systems.
 */

/**
 *
 */
struct bu_vls  {
    unsigned long vls_magic;
    char *vls_str;	/**< @brief Dynamic memory for buffer */
    int vls_offset;	/**< @brief Offset into vls_str where data is good */
    int vls_len;	/**< @brief Length, not counting the null */
    int vls_max;
};
#define BU_CK_VLS(_vp)		BU_CKMAG(_vp, BU_VLS_MAGIC, "bu_vls")
#define BU_VLS_IS_INITIALIZED(_vp)	\
    (((struct bu_vls *)(_vp) != (struct bu_vls *)0) && ((_vp)->vls_magic == BU_VLS_MAGIC))

/** @} */

/*----------------------------------------------------------------------*/
/** @addtogroup vlb */
/** @{ */
/** @file vlb.c
 *
 * @brief The variable length buffer package.
 *
 * The variable length buffer package.
 *
 */

/**
 * Variable Length Buffer: bu_vlb support
 */
struct bu_vlb {
    unsigned long magic;
    unsigned char *buf;  /**< @brief Dynamic memory for the buffer */
    int bufCapacity;     /**< @brief Current capacity of the buffer */
    int nextByte;        /**< @brief Number of bytes currently used in the buffer */
};
#define BU_VLB_BLOCK_SIZE 512
#define BU_CK_VLB(_vp)		BU_CKMAG(_vp, BU_VLB_MAGIC, "bu_vlb")
#define BU_VLB_IS_INITIALIZED(_vp)	\
	((_vp) && ((_vp)->magic == BU_VLB_MAGIC))
/** @} */
/*----------------------------------------------------------------------*/

/*
 * Section for manifest constants for bu_semaphore_acquire()
 */
#define BU_SEM_SYSCALL 0
#define BU_SEM_LISTS 1
#define BU_SEM_BN_NOISE 2
#define BU_SEM_MAPPEDFILE 3
#define BU_SEM_LAST (BU_SEM_MAPPEDFILE+1)	/* allocate this many for LIBBU+LIBBN */
/*
 * Automatic restart capability in bu_bomb().  The return from
 * BU_SETJUMP is the return from the setjmp().  It is 0 on the first
 * pass through, and non-zero when re-entered via a longjmp() from
 * bu_bomb().  This is only safe to use in non-parallel applications.
 */
#define BU_SETJUMP setjmp((bu_setjmp_valid=1, bu_jmpbuf))
#define BU_UNSETJUMP (bu_setjmp_valid=0)
/* These are global because BU_SETJUMP must be macro.  Please don't touch. */
BU_EXPORT extern int bu_setjmp_valid;		/* !0 = bu_jmpbuf is valid */
BU_EXPORT extern jmp_buf bu_jmpbuf;			/* for BU_SETJMP() */
/** @} */
/*-------------------------------------------------------------------------*/
/** @addtogroup mro */
/** @{ */
/**
 * B U _ M R O
 *
 * Support for Multiply Represented Objects
 */

struct bu_mro {
    unsigned long magic;
    struct bu_vls string_rep;
    char long_rep_is_valid;
    long long_rep;
    char double_rep_is_valid;
    double double_rep;
};

#define BU_CK_MRO(_vp)		BU_CKMAG(_vp, BU_MRO_MAGIC, "bu_mro")

#define BU_MRO_INVALIDATE(_p) {\
	_p->long_rep_is_valid = '\0';\
	_p->double_rep_is_valid = '\0';\
}

#define BU_MRO_GETDOUBLE(_p) (_p->double_rep_is_valid ? _p->double_rep : \
	(_p->double_rep = strtod(bu_vls_addr(&_p->string_rep), NULL), \
	(_p->double_rep_is_valid='y', _p->double_rep)))

#define BU_MRO_GETLONG(_p) (_p->long_rep_is_valid ? _p->long_rep : \
	(_p->long_rep = strtol(bu_vls_addr(&_p->string_rep), NULL, 10), \
	(_p->long_rep_is_valid='y', _p->long_rep)))

#define BU_MRO_GETSTRING(_p) bu_vls_addr(&_p->string_rep)

#define BU_MRO_STRLEN(_p) bu_vls_strlen(&_p->string_rep)
/** @} */

/*----------------------------------------------------------------------*/
/**
 * @addtogroup bu_debug Debugging
 * @ingroup libbu
 * @{
 */

/**
 * controls the libbu debug level
 */
BU_EXPORT extern int bu_debug;

/**
 * Section for BU_DEBUG values
 *
 * These can be set from the command-line of RT-compatible programs
 * using the "-!" option.
 *
 * These definitions are each for one bit.
 */
#define BU_DEBUG_OFF 0	/* No debugging */

#define BU_DEBUG_COREDUMP	0x00000001	/* bu_bomb() should dump core on exit */
#define BU_DEBUG_MEM_CHECK	0x00000002	/* Mem barrier & leak checking */
#define BU_DEBUG_MEM_LOG	0x00000004	/* Print all dynamic memory operations */
#define BU_DEBUG_DB		0x00000008	/* Database debug logging */

#define BU_DEBUG_PARALLEL	0x00000010	/* Parallel debug logging */
#define BU_DEBUG_MEM_QCHECK	0x00000020	/* Fast mem leak checking (won't work with corruption) */
#define BU_DEBUG_BACKTRACE	0x00000040	/* Log backtrace details during abnormal exit */
#define BU_DEBUG_ATTACH		0x00000080	/* Waits for a debugger to attach during a crash */

#define BU_DEBUG_MATH		0x00000100	/* Fundamental math routines (plane.c, mat.c) */
#define BU_DEBUG_PTBL		0x00000200	/* bu_ptbl_*() logging */
#define BU_DEBUG_AVS		0x00000400	/* bu_avs_*() logging */
#define BU_DEBUG_MAPPED_FILE	0x00000800	/* bu_mapped_file logging */

#define BU_DEBUG_PATHS		0x00001000	/* File and path debug logging */
#define BU_DEBUG_UNUSED_1	0x00002000	/* unused */
#define BU_DEBUG_UNUSED_2	0x00004000	/* unused */
#define BU_DEBUG_UNUSED_3	0x00008000	/* unused */

#define BU_DEBUG_TABDATA	0x00010000	/* LIBBN: tabdata */
#define BU_DEBUG_UNUSED_4	0x00020000	/* unused */
#define BU_DEBUG_UNUSED_5	0x00040000	/* unused */
#define BU_DEBUG_UNUSED_6	0x00080000	/* unused */

/* Format string for bu_printb() */
#define BU_DEBUG_FORMAT	\
"\020\
\025TABDATA\
\015?\
\014MAPPED_FILE\013AVS\012PTBL\011MATH\010?\7?\6MEM_QCHECK\5PARALLEL\
\4?\3MEM_LOG\2MEM_CHECK\1COREDUMP"

/** @} */
/*----------------------------------------------------------------------*/
/* parse.c */
/** @addtogroup parse */
/** @{ */
/*
 * Structure parse/print
 *
 * Definitions and data structures needed for routines that assign
 * values to elements of arbitrary data structures, the layout of
 * which is described by tables of "bu_structparse" structures.
 */

/**
 * b u _ o f f s e t o f
 * b u _ o f f s e t o f a r r a y
 *
 * The general problem of word-addressed hardware where (int *) and
 * (char *) have different representations is handled in the parsing
 * routines that use sp_offset, because of the limitations placed on
 * compile-time initializers.
 *
 * Files using bu_offsetof or bu_offsetofarray will need to include
 * stddef.h in order to get offsetof()
 */
/* FIXME - this is a temporary cast. The bu_structparse sp_offset member
 *         should be a size_t.
 */
#ifndef offsetof
#  define bu_offsetof(_t, _m) (size_t)(&(((_t *)0)->_m))
#  define bu_offsetofarray(_t, _m) (size_t)((((_t *)0)->_m))
#else
#  define bu_offsetof(_t, _m) (long)offsetof(_t, _m)
#  define bu_offsetofarray(_t, _m) (long)offsetof(_t, _m[0])
#endif


/**
 * b u _ b y t e o f f s e t
 *
 * Convert address of global data object into byte "offset" from
 * address 0.
 *
 * Strictly speaking, the C language only permits initializers of the
 * form: address +- constant, where here the intent is to measure the
 * byte address of the indicated variable.  Matching compensation code
 * for the CRAY is located in librt/parse.c
 */
#if defined(CRAY)
#	define bu_byteoffset(_i)	(((size_t)&(_i)))	/* actually a word offset */
#else
#  if defined(IRIX) && IRIX > 5 && _MIPS_SIM != _ABIN32 && _MIPS_SIM != _MIPS_SIM_ABI32
#      define bu_byteoffset(_i)	((size_t)__INTADDR__(&(_i)))
#  else
#    if defined(sgi) || defined(__convexc__) || defined(ultrix) || defined(_HPUX_SOURCE)
/* "Lazy" way.  Works on reasonable machines with byte addressing */
#      define bu_byteoffset(_i)	((size_t)((char *)&(_i)))
#    else
#      if defined(__ia64__) || defined(__x86_64__) || defined(__sparc64__)
#        if defined (__INTEL_COMPILER)
#          define bu_byteoffset(_i)	((size_t)((char *)&(_i)))
#        else
#          define bu_byteoffset(_i)	((size_t)(((void *)&(_i))-((void *)0)))
#        endif
#      else
/* "Conservative" way of finding # bytes as diff of 2 char ptrs */
#        define bu_byteoffset(_i)	((size_t)(((char *)&(_i))-((char *)0)))
#      endif
#    endif
#  endif
#endif

/**
 * The "bu_structparse" struct describes one element of a structure.
 * Collections of these are combined to describe entire structures (or at
 * least those portions for which parse/print/import/export support is
 * desired.  For example:
 *@code

 struct data_structure {
   char a_char;
   char str[32];
   short a_short;
   int a_int;
   double a_double;
 }

 struct data_structure default =
 { 'c', "the default string", 32767, 1, 1.0 };

 struct data_structure my_values;

 struct bu_structparse data_sp[] ={
   {"%c", 1,     "a_char",   bu_offsetof(data_structure, a_char), BU_STRUCTPARSE_FUNC_NULL,                      "a single character", (void*)&default.a_char},
   {"%s", 32,       "str", bu_offsetofarray(data_structure, str), BU_STRUCTPARSE_FUNC_NULL,         "This is a full character string", (void*)default.str},
   {"%i", 1,    "a_short",  bu_offsetof(data_structure, a_short), BU_STRUCTPARSE_FUNC_NULL,                         "A 16bit integer", (void*)&default.a_short},
   {"%d", 1,      "a_int",    bu_offsetof(data_structure, a_int), BU_STRUCTPARSE_FUNC_NULL,                          "A full integer", (void*)&default.a_int},
   {"%f", 1,   "a_double", bu_offsetof(data_structure, a_double), BU_STRUCTPARSE_FUNC_NULL, "A double-precision floating point value", (void*)&default.a_double},
   {  "", 0, (char *)NULL,                                     0, BU_STRUCTPARSE_FUNC_NULL,                              (char *)NULL, (void *)NULL}
 };

 @endcode
 *
 * To parse a string, call:
 *
 * bu_struct_parse(vls_string, data_sp, (char *)my_values)
 *
 * this will parse the vls string and assign values to the members of
 * the structure my_values
 *
 * A gross hack: To set global variables (or others for that matter)
 * you can store the actual address of the variable in the sp_offset
 * field and pass a null pointer as the last argument to
 * bu_struct_parse.  If you don't understand why this would work, you
 * probably shouldn't use this technique.
 */
struct bu_structparse {
    char sp_fmt[4];		/**< @brief "%i" or "%f", etc */
    size_t sp_count;		/**< @brief number of elements */
    char *sp_name;		/**< @brief Element's symbolic name */
    size_t sp_offset;		/**< @brief Byte offset in struct */
    void (*sp_hook)();	/**< @brief Optional hooked function, or indir ptr */
    char *sp_desc;		/**< @brief description of element */
    void *sp_default;		/**< @brief ptr to default value */
};
#define BU_STRUCTPARSE_FUNC_NULL ((void (*)())0)


/*----------------------------------------------------------------------*/
/**
 * B U _ E X T E R N A L
 *
 * An "opaque" handle for holding onto objects, typically in some kind
 * of external form that is not directly usable without passing
 * through an "importation" function.
 *
 * A "bu_external" struct holds the "external binary" representation
 * of a structure or other block of arbitrary data.
 */
struct bu_external  {
    unsigned long ext_magic;
    size_t ext_nbytes;
    genptr_t ext_buf;
};
#define BU_INIT_EXTERNAL(_p) {(_p)->ext_magic = BU_EXTERNAL_MAGIC; \
	(_p)->ext_buf = (genptr_t)NULL; (_p)->ext_nbytes = 0;}
#define BU_CK_EXTERNAL(_p)	BU_CKMAG(_p, BU_EXTERNAL_MAGIC, "bu_external")

/** @} */
/*----------------------------------------------------------------------*/
/* color.c */
#define HUE 0
#define SAT 1
#define VAL 2
#define ACHROMATIC	-1.0

struct bu_color
{
    unsigned long buc_magic;
    fastf_t buc_rgb[3];
};
#define BU_COLOR_NULL ((struct bu_color *) 0)
#define BU_CK_COLOR(_bp)	BU_CKMAG(_bp, BU_COLOR_MAGIC, "bu_color")

/*----------------------------------------------------------------------*/
/* red-black tree support */
/** @addtogroup rb */
/** @{ */
/*
 * The data structures and constants for red-black trees.
 *
 * Many of these routines are based on the algorithms in chapter 13 of
 * T. H. Cormen, C. E. Leiserson, and R. L. Rivest, "_Introduction to
 * algorithms", MIT Press, Cambridge, MA, 1990.
 *
 */

/**
 * B U _ R B _ L I S T
 *
 * List of nodes or packages
 *
 * The red-black tree package uses this structure to maintain lists of
 * all the nodes and all the packages in the tree.  Applications
 * should not muck with these things.  They are maintained only to
 * facilitate freeing bu_rb_trees.
 */
struct bu_rb_list
{
    struct bu_list l;
    union
    {
	struct bu_rb_node *rbl_n;
	struct bu_rb_package *rbl_p;
    }			rbl_u;
};
#define rbl_magic l.magic
#define rbl_node rbl_u.rbl_n
#define rbl_package rbl_u.rbl_p
#define BU_RB_LIST_NULL ((struct bu_rb_list *) 0)


/**
 * B U _ R B _ T R E E
 *
 * This is the only data structure used in the red-black tree package
 * to which application software need make any explicit reference.
 *
 * The members of this structure are grouped into three classes:
 *
 * Class I:	Reading is appropriate, when necessary,
 *		but applications should not modify.
 * Class II:	Reading and modifying are both appropriate,
 *		when necessary.
 * Class III:	All access should be through routines
 *		provided in the package.  Touch these
 *		at your own risk!
 */
typedef struct
{
    /* CLASS I - Applications may read directly. */
    unsigned long rbt_magic;           /**< @brief Magic no. for integrity check */
    int rbt_nm_nodes;                  /**< @brief Number of nodes */
    /* CLASS II - Applications may read/write directly. */
    void (*rbt_print)(void *);         /**< @brief Data pretty-print function */
    int rbt_debug;                     /**< @brief Debug bits */
    char *rbt_description;             /**< @brief Comment for diagnostics */
    /* CLASS III - Applications should not manipulate directly. */
    int rbt_nm_orders;                 /**< @brief Number of simultaneous orders */
    int (**rbt_order)();               /**< @brief Comparison functions */
    struct bu_rb_node **rbt_root;      /**< @brief The actual trees */
    char *rbt_unique;                  /**< @brief Uniqueness flags */
    struct bu_rb_node *rbt_current;    /**< @brief Current node */
    struct bu_rb_list rbt_nodes;       /**< @brief All nodes */
    struct bu_rb_list rbt_packages;    /**< @brief All packages */
    struct bu_rb_node *rbt_empty_node; /**< @brief Sentinel representing nil */
}	bu_rb_tree;
#define BU_RB_TREE_NULL ((bu_rb_tree *) 0)

/*
 * Debug bit flags for member rbt_debug
 */
#define BU_RB_DEBUG_INSERT 0x00000001	/**< @brief Insertion process */
#define BU_RB_DEBUG_UNIQ 0x00000002	/**< @brief Uniqueness of inserts */
#define BU_RB_DEBUG_ROTATE 0x00000004	/**< @brief Rotation process */
#define BU_RB_DEBUG_OS 0x00000008	/**< @brief Order-statistic operations */
#define BU_RB_DEBUG_DELETE 0x00000010	/**< @brief Deletion process */

/**
 * B U _ R B _ P A C K A G E
 *
 * Wrapper for application data
 *
 * This structure provides a level of indirection between the
 * application software's data and the red-black nodes in which the
 * data is stored.  It is necessary because of the algorithm for
 * deletion, which generally shuffles data among nodes in the tree.
 * The package structure allows the application data to remember which
 * node "contains" it for each order.
 */
struct bu_rb_package
{
    unsigned long rbp_magic;	/**< @brief Magic no. for integrity check */
    struct bu_rb_node **rbp_node;	/**< @brief Containing nodes */
    struct bu_rb_list *rbp_list_pos;	/**< @brief Place in the list of all pkgs.  */
    void *rbp_data;	/**< @brief Application data */
};
#define BU_RB_PKG_NULL ((struct bu_rb_package *) 0)

/**
 * B U _ R B _ N O D E
 *
 * For the most part, there is a one-to-one correspondence between
 * nodes and chunks of application data.  When a node is created, all
 * of its package pointers (one per order of the tree) point to the
 * same chunk of data.  However, subsequent deletions usually muddy
 * this tidy state of affairs.
 */
struct bu_rb_node
{
    unsigned long rbn_magic;		/**< @brief Magic no. for integrity check */
    bu_rb_tree *rbn_tree;		/**< @brief Tree containing this node */
    struct bu_rb_node **rbn_parent;	/**< @brief Parents */
    struct bu_rb_node **rbn_left;	/**< @brief Left subtrees */
    struct bu_rb_node **rbn_right;	/**< @brief Right subtrees */
    char *rbn_color;			/**< @brief Colors of this node */
    int *rbn_size;			/**< @brief Sizes of subtrees rooted here */
    struct bu_rb_package **rbn_package;	/**< @brief Contents of this node */
    int rbn_pkg_refs;			/**< @brief How many orders are being used?  */
    struct bu_rb_list *rbn_list_pos;	/**< @brief Place in the list of all nodes */
};
#define BU_RB_NODE_NULL ((struct bu_rb_node *) 0)

/*
 * Applications interface to bu_rb_extreme()
 */
#define SENSE_MIN 0
#define SENSE_MAX 1
#define bu_rb_min(t, o) bu_rb_extreme((t), (o), SENSE_MIN)
#define bu_rb_max(t, o) bu_rb_extreme((t), (o), SENSE_MAX)
#define bu_rb_pred(t, o) bu_rb_neighbor((t), (o), SENSE_MIN)
#define bu_rb_succ(t, o) bu_rb_neighbor((t), (o), SENSE_MAX)

/*
 * Applications interface to bu_rb_walk()
 */
#define PREORDER	0
#define INORDER		1
#define POSTORDER	2


/**
 * B U _ O B S E R V E R
 */
struct bu_observer {
    struct bu_list l;
    struct bu_vls observer;
    struct bu_vls cmd;
};
#define BU_OBSERVER_NULL ((struct bu_observer *)0)

/**
 * B U _ C M D T A B
 */
struct bu_cmdtab {
    char *ct_name;
    int (*ct_func)();
};

/*----------------------------------------------------------------------*/
/* Miscellaneous macros */
#define bu_made_it()		bu_log("Made it to %s:%d\n",	\
					__FILE__, __LINE__)
/*----------------------------------------------------------------------*/
/*
 * Declarations of external functions in LIBBU.  Source file names
 * listed alphabetically.
 */
/**@}*/

/** @addtogroup avs */
/** @{ */
/* avs.c */


/**
 * B U _ A V S _ I N I T
 *
 * initialize avs with storage for len entries
 */
BU_EXPORT BU_EXTERN(void bu_avs_init,
		    (struct bu_attribute_value_set *avp,
		     int len,
		     const char *str));

/**
 * B U _ A V S _ I N I T _ E M P T Y
 *
 * initialize an empty avs
 */
BU_EXPORT BU_EXTERN(void bu_avs_init_empty,
		    (struct bu_attribute_value_set *avp));

/**
 * B U _ A V S _ N E W
 *
 * Allocate storage for a new attribute/value set, with at least 'len'
 * slots pre-allocated.
 */
BU_EXPORT BU_EXTERN(struct bu_attribute_value_set *bu_avs_new,
		    (int len,
		     const char *str));

/**
 * B U _ A V S _ A D D
 *
 * If the given attribute exists it will recieve the new value,
 * othwise the set will be extended to have a new attribute/value
 * pair.
 *
 * Returns -
 * 0 some error occured
 * 1 existing attribute updated with new value
 * 2 set extended with new attribute/value pair
 */
BU_EXPORT BU_EXTERN(int bu_avs_add,
		    (struct bu_attribute_value_set *avp,
		     const char *attribute,
		     const char *value));

/**
 * B U _ A V S _ A D D _ V L S
 *
 * Add a bu_vls string as an attribute to a given attribute set.
 */
BU_EXPORT BU_EXTERN(int bu_avs_add_vls,
		    (struct bu_attribute_value_set *avp,
		     const char *attribute,
		     const struct bu_vls *value_vls));

/**
 * B U _ A V S _ M E R G E
 *
 * Take all the attributes from 'src' and merge them into 'dest' by
 * replacing an attribute if it already exists.
 */
BU_EXPORT BU_EXTERN(void bu_avs_merge,
		    (struct bu_attribute_value_set *dest,
		     const struct bu_attribute_value_set *src));

/**
 * B U _ A V S _ G E T
 *
 * Get the value of a given attribute from an attribute set.
 */
BU_EXPORT BU_EXTERN(const char *bu_avs_get,
		    (const struct bu_attribute_value_set *avp,
		     const char *attribute));

/**
 * B U _ A V S _ R E M O V E
 *
 * Remove the given attribute from an attribute set.
 *
 * @return
 *	-1	attribute not found in set
 * @return
 *	 0	OK
 */
BU_EXPORT BU_EXTERN(int bu_avs_remove,
		    (struct bu_attribute_value_set *avp,
		     const char *attribute));

/**
 * B U _ A V S _ F R E E
 *
 * Release all attributes in an attribute set.
 */
BU_EXPORT BU_EXTERN(void bu_avs_free,
		    (struct bu_attribute_value_set *avp));

/**
 * B U _ A V S _ P R I N T
 *
 * Print all attributes in an attribute set in "name = value" form,
 * using the provided title.
 */
BU_EXPORT BU_EXTERN(void bu_avs_print,
		    (const struct bu_attribute_value_set *avp,
		     const char *title));

/**
 * B U _ A V S _ A D D _ N O N U N I Q U E
 *
 * Add a name/value pair even if the name already exists in this AVS.
 */
BU_EXPORT BU_EXTERN(void bu_avs_add_nonunique,
		    (struct bu_attribute_value_set *avsp,
		     const char *attribute,
		     const char *value));
/** @} */

/** @addtogroup bitv */
/** @{ */

/**
 * B U _ B I T V _ N E W
 * @brief
 * Allocate storage for a new bit vector of at least 'nbits' in
 * length.  The bit vector itself is guaranteed to be initialized to
 * all zero.
 */
BU_EXPORT BU_EXTERN(struct bu_bitv *bu_bitv_new, (unsigned int nbits));

/**
 * B U _ B I T V _ F R E E
 * @brief
 * Release all internal storage for this bit vector.
 *
 * It is the caller's responsibility to not use the pointer 'bv' any
 * longer.  It is the caller's responsibility to dequeue from any
 * linked list first.
 */
BU_EXPORT BU_EXTERN(void bu_bitv_free, (struct bu_bitv *bv));

/**
 * B U _ B I T V _ C L E A R
 * @brief
 * Set all the bits in the bit vector to zero.
 *
 * Also available as a BU_BITV_ZEROALL macro if you don't desire the
 * pointer checking.
 */
BU_EXPORT BU_EXTERN(void bu_bitv_clear, (struct bu_bitv *bv));

/**
 * B U _ B I T V _ O R
 */
BU_EXPORT BU_EXTERN(void bu_bitv_or, (struct bu_bitv *ov,  const struct bu_bitv *iv));

/**
 * B U _ B I T V _ A N D
 */
BU_EXPORT BU_EXTERN(void bu_bitv_and, (struct bu_bitv *ov, const struct bu_bitv *iv));

/**
 * B U _ B I T V _ V L S
 * @brief
 * Print the bits set in a bit vector.
 */
BU_EXPORT BU_EXTERN(void bu_bitv_vls, (struct bu_vls *v, const struct bu_bitv *bv));

/**
 * B U _ P R _ B I T V
 * @brief
 * Print the bits set in a bit vector.  Use bu_vls stuff, to make only
 * a single call to bu_log().
 */
BU_EXPORT BU_EXTERN(void bu_pr_bitv, (const char *str, const struct bu_bitv *bv));

/**
 * B U _ B I T V _ T O _ H E X
 * @brief
 * Convert a bit vector to an ascii string of hex digits.  The string
 * is from MSB to LSB (bytes and bits).
 */
BU_EXPORT BU_EXTERN(void bu_bitv_to_hex, (struct bu_vls *v, const struct bu_bitv *bv));

/**
 * B U _ H E X _ T O _ B I T V
 * @brief
 * Convert a string of HEX digits (as produces by bu_bitv_to_hex) into
 * a bit vector.
 */
BU_EXPORT BU_EXTERN(struct bu_bitv *bu_hex_to_bitv, (const char *str));

/**
 * B U _ B I T V _ D U P
 * @brief
 * Make a copy of a bit vector
 */
BU_EXPORT BU_EXTERN(struct bu_bitv *bu_bitv_dup, (const struct bu_bitv *bv));


/** @} */

/** @addtogroup bu_log */
/** @{ */
/** @file backtrace.c
 *
 * Extract a backtrace of the current call stack.
 *
 */

/**
 * b u _ b a c k t r a c e
 *
 * this routine provides a trace of the call stack to the caller,
 * generally called either directly, via a signal handler, or through
 * bu_bomb() with the appropriate bu_debug flags set.
 *
 * the routine waits indefinitely (in a spin loop) until a signal
 * (SIGINT) is received, at which point execution continues, or until
 * some other signal is received that terminates the application.
 *
 * the stack backtrace will be written to the provided 'fp' file
 * pointer.  it's the caller's responsibility to open and close
 * that pointer if necessary.  If 'fp' is NULL, stdout will be used.
 *
 * returns truthfully if a backtrace was attempted.
 */
BU_EXPORT BU_EXTERN(int bu_backtrace, (FILE *fp));

/**
 * B U _ B O M B
 * @brief
 * Abort the running process.
 *
 * The bu_bomb routine is called on a fatal error, generally where no
 * recovery is possible.  Error handlers may, however, be registered
 * with BU_SETJMP.  This routine intentionally limits calls to other
 * functions and intentionally uses no stack variables.  Just in case
 * the application is out of memory, bu_bomb deallocates a small
 * buffer of memory.
 *
 * Before termination, it optionally performs the following operations
 * in the order listed:
 *
 * 1. Outputs str to standard error
 *
 * 2. Calls any callback functions set in the global bu_bomb_hook_list
 *    variable with str passed as an argument.
 *
 * 3. Jumps to any user specified error handler registered with the
 *    bu_setjmp_valid/bu_jmpbuf setjmp facility.
 *
 * 4. Outputs str to the terminal device in case standard error is
 *    redirected.
 *
 * 5. Aborts abnormally (via abort()) if BU_DEBUG_COREDUMP is defined.
 *
 * 6. Exits with exit(12).
 *
 * Only produce a core-dump when that debugging bit is set.  Note that
 * this function is meant to be a last resort semi-graceful abort.
 *
 * This routine should never return unless there is a bu_setjmp
 * handler registered.
 */
BU_EXPORT BU_EXTERN(void bu_bomb, (const char *str)) __BU_ATTR_NORETURN;

/**
 * b u _ e x i t
 *
 * Semi-graceful termination of the application that doesn't cause a
 * stack trace, exiting with the specified status after printing the
 * given message.  It's okay for this routine to use the stack,
 * contrary to bu_bomb's behavior since it should be called for
 * expected termination situations.
 *
 * This routine should generally not be called within a library.  Use
 * bu_bomb or (better) cascade the error back up to the application.
 *
 * This routine should never return.
 */
BU_EXPORT BU_EXTERN(void bu_exit, (int status, const char *fmt, ...)) __BU_ATTR_NORETURN __BU_ATTR_FORMAT23;

/** @file crashreport.c
 *
 * Generate a crash report file, including a call stack backtrace and
 * other system details.
 *
 */

/**
 * b u _ c r a s h r e p o r t
 *
 * this routine writes out details of the currently running process to
 * the specified file, including an informational header about the
 * execution environment, stack trace details, kernel and hardware
 * information, and current version information.
 *
 * returns truthfully if the crash report was written.
 *
 * due to various reasons, this routine is NOT thread-safe.
 */
BU_EXPORT BU_EXTERN(int bu_crashreport, (const char *filename));

/** @file fgets.c
 *
 * fgets replacement function that also handles CR as an EOL marker
 *
 */

/**
 * b u _ f g e t s
 *
 * Reads in at most one less than size characters from stream and
 * stores them into the buffer pointed to by s. Reading stops after an
 * EOF, CR, LF, or a CR/LF combination. If a LF or CR is read, it is
 * stored into the buffer. If a CR/LF is read, just a CR is stored
 * into the buffer. A '\\0' is stored after the last character in the
 * buffer. Returns s on success, and NULL on error or when end of file
 * occurs while no characters have been read.
 */
BU_EXPORT BU_EXTERN(char *bu_fgets, (char *s, int size, FILE *stream));

/** @} */
/** @addtogroup color */
/** @{ */

/**
 * Convert between RGB and HSV color models
 *
 * R, G, and B are in {0, 1, ..., 255},
 *
 * H is in [0.0, 360.0), and S and V are in [0.0, 1.0],
 *
 * unless S = 0.0, in which case H = ACHROMATIC.
 *
 * These two routines are adapted from:
 * pp. 592-3 of J.D. Foley, A. van Dam, S.K. Feiner, and J.F. Hughes,
 * _Computer graphics: principles and practice_, 2nd ed., Addison-Wesley,
 * Reading, MA, 1990.
 */

/* color.c */
BU_EXPORT BU_EXTERN(void bu_rgb_to_hsv, (unsigned char *rgb, fastf_t *hsv));
BU_EXPORT BU_EXTERN(int bu_hsv_to_rgb, (fastf_t *hsv, unsigned char *rgb));
BU_EXPORT BU_EXTERN(int bu_str_to_rgb, (char *str, unsigned char *rgb));
BU_EXPORT BU_EXTERN(int bu_color_from_rgb_floats, (struct bu_color *cp, fastf_t *rgb));
BU_EXPORT BU_EXTERN(int bu_color_to_rgb_floats, (struct bu_color *cp, fastf_t *rgb));

/* UNIMPLEMENTED
 *
 * BU_EXPORT BU_EXTERN(void bu_color_from_rgb_chars,
 * 		    (struct bu_color *cp,
 * 		     unsigned char *rgb));
 * BU_EXPORT BU_EXTERN(int bu_color_to_rgb_chars,
 * 		    (struct bu_color *cp,
 * 		     unsigned char *rgb));
 * BU_EXPORT BU_EXTERN(int bu_color_from_hsv_floats,
 * 		    (struct bu_color *cp,
 * 		     fastf_t *hsv));
 * BU_EXPORT BU_EXTERN(int bu_color_to_hsv_floats,
 * 		    (struct bu_color *cp,
 * 		     fastf_t *hsv));
 */


/** @} */
/** @addtogroup file */
/** @{ */

/** @file stat.c
 *
 * Support routines for identifying properties of files and
 * directories such as whether they exist or are the same as another
 * given file.
 *
 */

/**
 * B U _ F I L E _ E X I S T S
 *
 * @return 1 The given filename exists.
 * @return 0 The given filename does not exist.
 */
BU_EXPORT BU_EXTERN(int bu_file_exists, (const char *path));

/**
 * b u _ s a m e _ f i l e
 *
 * returns truthfully as to whether or not the two provided filenames
 * are the same file.  if either file does not exist, the result is
 * false.
 */
BU_EXPORT BU_EXTERN(int bu_same_file, (const char *fn1, const char *fn2));

/**
 * b u _ s a m e _ f d
 *
 * returns truthfully as to whether or not the two provided file
 * descriptors are the same file.  if either file does not exist, the
 * result is false.
 */
BU_EXPORT BU_EXTERN(int bu_same_fd, (int fd1, int fd2));

/**
 * b u _ f i l e _ r e a d a b l e
 *
 * returns truthfully if current user can read the specified file or
 * directory.
 */
BU_EXPORT BU_EXTERN(int bu_file_readable, (const char *path));

/**
 * b u _ f i l e _ w r i t a b l e
 *
 * returns truthfully if current user can write to the specified file
 * or directory.
 */
BU_EXPORT BU_EXTERN(int bu_file_writable, (const char *path));

/**
 * b u _ f i l e _ e x e c u t a b l e
 *
 * returns truthfully if current user can run the specified file or
 * directory.
 */
BU_EXPORT BU_EXTERN(int bu_file_executable, (const char *path));

/** @file fnmatch.c
 *
 */

/**
 * Function fnmatch() as specified in POSIX 1003.2-1992, section B.6.
 * Compares a filename or pathname to a pattern.
 *
 */
BU_EXPORT BU_EXTERN(int bu_fnmatch, (const char *, const char *, int));

/** @file dirent.c
 *
 * Functionality for accessing all files in a directory.
 *
 */

/**
 * Count number of files in directory whose type matches substr
 */
BU_EXPORT BU_EXTERN(int bu_count_path, (char *path, char *substr));

/**
 * Return array with filenames with suffix matching substr
 */
BU_EXPORT BU_EXTERN(void bu_list_path, (char *path, char *substr, char **filearray)); 


/** @file brlcad_path.c
 *
 * @brief
 * A support routine to provide the executable code with the path
 * to where the BRL-CAD programs and libraries are installed.
 *
 */

/**
 * b u _ a r g v 0
 *
 * this routine is used by the brlcad-path-finding routines when
 * attempting to locate binaries, libraries, and resources.  This
 * routine will set argv0 if path is provided and should generally be
 * set early on by bu_setprogname().
 *
 * this routine will return "(unknown)" if argv[0] cannot be
 * identified but should never return NULL.
 *
 * DEPRECATED: This routine is replaced by bu_argv0_full_path().
 *             Do not use.
 */
DEPRECATED BU_EXPORT BU_EXTERN(const char *bu_argv0, (void));

/**
 * b u _ a r g v 0 _ f u l l _ p a t h
 *
 * returns the full path to argv0, regardless of how the application
 * was invoked.
 *
 * this routine will return "(unknown)" if argv[0] cannot be
 * identified but should never return NULL.
 */
BU_EXPORT BU_EXTERN(const char *bu_argv0_full_path, (void));

/**
 * b u _ g e t p r o g n a m e
 *
 * get the name of the running application if they ran
 * bu_setprogname() first or if we know what it's supposed to be
 * anyways.
 */
BU_EXPORT BU_EXTERN(const char *bu_getprogname, (void));

/**
 * b u _ s e t p r o g n a m e
 *
 * Set the name of the running application.  This isn't necessary on
 * modern systems that support getprogname() and call setprogname()
 * before main() for you, but necessary otherwise for portability.
 */
BU_EXPORT BU_EXTERN(void bu_setprogname, (const char *path));

/**
 * b u _ b r l c a d _ r o o t
 *
 * Locate where the BRL-CAD applications and libraries are installed.
 *
 * The BRL-CAD root is searched for in the following order of
 * precedence by testing for the rhs existence if provided or the
 * directory existence otherwise:
 *
 *   BRLCAD_ROOT environment variable if set
 *   BRLCAD_ROOT compile-time path
 *   run-time path identification
 *   /usr/brlcad static path
 *   current directory
 *
 * @return
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
BU_EXPORT BU_EXTERN(const char *bu_brlcad_root, (const char *rhs, int fail_quietly));

/**
 * b u _ b r l c a d _ d a t a
 *
 * Locate where the BRL-CAD data resources are installed.
 *
 * The BRL-CAD data resources are searched for in the following order
 * of precedence by testing for the existence of rhs if provided or
 * the directory existence otherwise:
 *
 *   BRLCAD_DATA environment variable if set
 *   BRLCAD_DATA compile-time path
 *   bu_brlcad_root/share/brlcad/VERSION path
 *   bu_brlcad_root path
 *   current directory
 *
 * A STATIC buffer is returned.  It is the caller's responsibility to
 * call bu_strdup() or make other provisions to save the returned
 * string, before calling again.
 */
BU_EXPORT BU_EXTERN(const char *bu_brlcad_data, (const char *rhs, int fail_quietly));

/**
 * b u _ w h i c h
 *
 * returns the first USER path match to a given executable name.
 *
 * Routine to provide BSD "which" functionality, locating binaries of
 * specified programs from the user's PATH. This is useful to locate
 * binaries and resources at run-time.
 *
 * caller should not free the result, though it will not be preserved
 * between calls either.  the caller should strdup the result if they
 * need to keep it around.
 *
 * routine will return NULL if the executable command cannot be found.
 */
BU_EXPORT BU_EXTERN(const char *bu_which, (const char *cmd));

/**
 * b u _ w h e r e i s
 *
 * returns the first SYSTEM path match to a given executable cmd name.
 *
 * Routine to provide BSD "whereis" functionality, locating binaries
 * of specified programs from the SYSTEM path.  This is useful to
 * locate binaries and resources at run-time.
 *
 * caller should not free the result, though it will not be preserved
 * between calls either.  the caller should strdup the result if they
 * need to keep it around.
 *
 * routine will return NULL if the executable command cannot be found.
 */
BU_EXPORT BU_EXTERN(const char *bu_whereis, (const char *cmd));

/** @file fopen_uniq.c
 *
 * @brief DEPRECATED: Routine to open a unique filename.
 *
 */

/**
 * B U _ F O P E N _ U N I Q
 *
 * @brief Open a file for output.  Assures that the file did not previously
 * exist.
 *
 * This routine is DEPRECATED.  Do not use.
 *
 * Typical usage:
 @code
 *	static int n = 0;
 *	FILE *fp;
 *
 *	fp = bu_fopen_uniq("writing to %s for results", "output%d.pl", n++);
 *	...
 *	fclose(fp);
 *
 *
 *	fp = bu_fopen_uniq((char *)NULL, "output%d.pl", n++);
 *	...
 *	fclose(fp);
 @endcode
 *
 * DEPRECATED
 */
DEPRECATED BU_EXPORT BU_EXTERN(FILE *bu_fopen_uniq, (const char *outfmt, const char *namefmt, int n));

/** @file temp.c
 *
 * Routine to open a temporary file.
 *
 */

/**
 * b u _ t e m p _ f i l e
 *
 * Create a temporary file.  The first readable/writable directory
 * will be used, searching TMPDIR/TEMP/TMP environment variable
 * directories followed by default system temp directories and
 * ultimately trying the current directory.
 *
 * This routine is guaranteed to return a new unique file or return
 * NULL on failure.  The temporary file will be automatically unlinked
 * on application exit.  It is the caller's responsibility to set file
 * access settings, preserve file contents, or destroy file contents
 * if the default behavior is non-optimal.
 *
 * The name of the temporary file will be copied into a user-provided
 * (filepath) buffer if it is a non-NULL pointer and of a sufficient
 * (len) length to contain the filename.
 *
 * This routine is NOT thread-safe.
 *
 * Typical Use:
 @code
 * FILE *fp;
 * char filename[MAXPATHLEN];
 * fp = bu_temp_file(&filename, MAXPATHLEN); // get file name
 * ...
 * fclose(fp); // optional, auto-closed on exit
 *
 * ...
 *
 * fp = bu_temp_file(NULL, 0); // don't need file name
 *      fchmod(fileno(fp), 0777);
 * ...
 * rewind(fp);
 * while (fputc(0, fp) == 0);
 * fclose(fp);
 @endcode
*/
BU_EXPORT BU_EXTERN(FILE *bu_temp_file, (char *filepath, size_t len));

/** @} */
/** @addtogroup getopt */
/** @{ */

/** @file getopt.c
 *
 * @brief
 * Special re-entrant version of getopt.
 *
 * Everything is prefixed with bu_, to distinguish it from the various
 * getopt routines found in libc.
 *
 * Important note -
 * If bu_getopt() it going to be used more than once, it is necessary
 * to reinitialize bu_optind=1 before beginning on the next argument
 * list.
 */

/**
 * for bu_getopt().  set to zero to suppress errors.
 */
BU_EXPORT extern int bu_opterr;

/**
 * for bu_getopt().  current index into parent argv vector.
 */
BU_EXPORT extern int bu_optind;

/**
 * for bu_getopt().  current option being checked for validity.
 */
BU_EXPORT extern int bu_optopt;

/**
 * for bu_getopt().  current argument associated with current option.
 */
BU_EXPORT extern char *bu_optarg;

/**
 * B U _ G E T O P T
 *
 * get option letter from argument vector
 */
BU_EXPORT BU_EXTERN(int bu_getopt, (int nargc, char * const nargv[], const char *ostr));

/** @} */
/** @addtogroup bu_hist */
/** @{ */

/* hist.c */

/**
 * B U _ H I S T _ F R E E
 */
BU_EXPORT BU_EXTERN(void bu_hist_free, (struct bu_hist *histp));

/**
 * B U _ H I S T _ I N I T
 *
 * Initialize a bu_hist structure.
 *
 * It is expected that the structure is junk upon entry.
 */
BU_EXPORT BU_EXTERN(void bu_hist_init, (struct bu_hist *histp, fastf_t min, fastf_t max, unsigned int nbins));

/**
 * B U _ H I S T _ R A N G E
 */
BU_EXPORT BU_EXTERN(void bu_hist_range, (struct bu_hist *hp, fastf_t low, fastf_t high));

/**
 * B U _ H I S T _ P R
 *
 * The original interface.
 */
BU_EXPORT BU_EXTERN(void bu_hist_pr, (const struct bu_hist *histp, const char *title));

/** @} */

/** @addtogroup hton */
/** @{ */
/** @file htond.c
 *
 * @brief
 * convert doubles to host/network format
 *
 * Library routines for conversion between the local host 64-bit
 * ("double precision") representation, and 64-bit IEEE double
 * precision representation, in "network order", ie, big-endian, the
 * MSB in byte [0], on the left.
 *
 * As a quick review, the IEEE double precision format is as follows:
 * sign bit, 11 bits of exponent (bias 1023), and 52 bits of mantissa,
 * with a hidden leading one (0.1 binary).
 *
 * When the exponent is 0, IEEE defines a "denormalized number", which
 * is not supported here.
 *
 * When the exponent is 2047 (all bits set), and:
 *	all mantissa bits are zero,
 *	value is infinity*sign,
 *	mantissa is non-zero, and:
 *		msb of mantissa=0:  signaling NAN
 *		msb of mantissa=1:  quiet NAN
 *
 * Note that neither the input or output buffers need be word aligned,
 * for greatest flexability in converting data, even though this
 * imposes a speed penalty here.
 *
 * These subroutines operate on a sequential block of numbers, to save
 * on subroutine linkage execution costs, and to allow some hope for
 * vectorization.
 *
 * On brain-damaged machines like the SGI 3-D, where type "double"
 * allocates only 4 bytes of space, these routines *still* return 8
 * bytes in the IEEE buffer.
 *
 */

/**
 * H T O N D
 *
 * @brief Host to Network Doubles
 */
BU_EXPORT BU_EXTERN(void htond,
		    (unsigned char *out,
		     const unsigned char *in,
		     size_t count));

/**
 * N T O H D
 *
 * @brief Network to Host Doubles
 */
BU_EXPORT BU_EXTERN(void ntohd,
		    (unsigned char *out,
		     const unsigned char *in,
		     size_t count));

/** @file htonf.c
 *
 * @brief convert floats to host/network format
 *
 * Host to Network Floats  +  Network to Host Floats.
 *
 */

/**
 * H T O N F
 *
 * Host to Network Floats
 */
BU_EXPORT BU_EXTERN(void htonf,
		    (unsigned char *out,
		     const unsigned char *in,
		     size_t count));

/**
 * N T O H F
 *
 * Network to Host Floats
 */
BU_EXPORT BU_EXTERN(void ntohf,
		    (unsigned char *out,
		     const unsigned char *in,
		     size_t count));

/** @} */

/** @addtogroup thread */
/** @{ */
/** @file ispar.c
 *
 * @brief
 * subroutine to determine if we are multi-threaded
 *
 * This subroutine is separated off from parallel.c so that bu_bomb()
 * and others can call it, without causing either parallel.c or
 * semaphore.c to get referenced and thus causing the loader to drag
 * in all the parallel processing stuff from the vendor library.
 *
 */

/**
 * B U _ I S _ P A R A L L E L
 *
 * A clean way for bu_bomb() to tell if this is a parallel
 * application.  If bu_parallel() is active, this routine will return
 * non-zero.
 */
BU_EXPORT BU_EXTERN(int bu_is_parallel,
		    ());

/**
 * B U _ K I L L _ P A R A L L E L
 *
 * Used by bu_bomb() to help terminate parallel threads,
 * without dragging in the whole parallel library if it isn't being used.
 */
BU_EXPORT BU_EXTERN(void bu_kill_parallel,
		    ());

/** @} */

/** @addtogroup bu_log */
/** @{ */
/** @file linebuf.c
 *
 * A portable way of doing setlinebuf().
 *
 */

BU_EXPORT BU_EXTERN(void bu_setlinebuf,
		    (FILE *fp));

/** @} */

/** @addtogroup bu_list */
/** @{ */

/**
 * B U _ L I S T _ N E W
 *
 * Creates and initializes a bu_list head structure
 */
BU_EXPORT BU_EXTERN(struct bu_list *bu_list_new, ());

/**
 * B U _ L I S T _ P O P
 *
 * Returns the results of BU_LIST_POP
 */
BU_EXPORT BU_EXTERN(struct bu_list *bu_list_pop, (struct bu_list *hp));

/**
 * B U _ L I S T _ L E N
 *
 * Returns the number of elements on a bu_list brand linked list.
 */
BU_EXPORT BU_EXTERN(int bu_list_len,
		    (const struct bu_list *hd));

/**
 * B U _ L I S T _ R E V E R S E
 *
 * Reverses the order of elements in a bu_list linked list.
 */
BU_EXPORT BU_EXTERN(void bu_list_reverse,
		    (struct bu_list *hd));

/**
 * B U _ L I S T _ F R E E
 *
 * Given a list of structures allocated with bu_malloc() enrolled
 * on a bu_list head, walk the list and free the structures.
 * This routine can only be used when the structures have no interior
 * pointers.
 */
BU_EXPORT BU_EXTERN(void bu_list_free,
		    (struct bu_list *hd));

/**
 * B U _ L I S T _ P A R A L L E L _ A P P E N D
 *
 * Simple parallel-safe routine for appending a data structure to the
 * end of a bu_list doubly-linked list.
 *
 * @par Issues:
 *  	Only one semaphore shared by all list heads.
 * @n	No portable way to notify waiting thread(s) that are sleeping
 */
BU_EXPORT BU_EXTERN(void bu_list_parallel_append,
		    (struct bu_list *headp,
		     struct bu_list *itemp));

/**
 * B U _ L I S T _ P A R A L L E L _ D E Q U E U E
 *
 * Simple parallel-safe routine for dequeueing one data structure from
 * the head of a bu_list doubly-linked list.
 * If the list is empty, wait until some other thread puts something on
 * the list.
 *
 * @par Issues:
 * No portable way to not spin and burn CPU time while waiting
 * @n	for something to show up on the list.
 */
BU_EXPORT BU_EXTERN(struct bu_list *bu_list_parallel_dequeue,
		    (struct bu_list *headp));

/**
 * B U _ C K _ L I S T
 *
 * Generic bu_list doubly-linked list checker.
 */
BU_EXPORT BU_EXTERN(void bu_ck_list,
		    (const struct bu_list *hd,
		     const char *str));

/**
 * B U _ C K _ L I S T _ M A G I C
 *
 * bu_list doubly-linked list checker which checks the magic number for
 * all elements in the linked list
 */
BU_EXPORT BU_EXTERN(void bu_ck_list_magic,
		    (const struct bu_list *hd,
		     const char *str,
		     const unsigned long magic));

/** @} */

/** @addtogroup bu_log */
/** @{ */
/** @file hook.c
 *
 * @brief
 * BRL-CAD support library's hook utility.
 *
 */
BU_EXPORT BU_EXTERN(void bu_hook_list_init,
		    (struct bu_hook_list *hlp));
BU_EXPORT BU_EXTERN(void bu_add_hook,
		    (struct bu_hook_list *hlp,
		     bu_hook_t func,
		     genptr_t clientdata));
BU_EXPORT BU_EXTERN(void bu_delete_hook,
		    (struct bu_hook_list *hlp,
		     bu_hook_t func,
		     genptr_t clientdata));
BU_EXPORT BU_EXTERN(void bu_call_hook,
		    (struct bu_hook_list *hlp,
		     genptr_t buf));

/** @} */
/** @addtogroup bu_log */
/** @{ */
/** @file log.c
 *
 * @brief
 * parallel safe version of fprintf for logging
 *
 * BRL-CAD support library, error logging routine.  Note that the user
 * may provide his own logging routine, by replacing these functions.
 * That is why this is in file of it's own.  For example, LGT and
 * RTSRV take advantage of this.
 *
 * Here is an example of how to set up a custom logging callback.
 * While bu_log presently writes to STDERR by default, this behavior
 * should not be relied upon and may be changed to STDOUT in the
 * future without notice.
 *
 * --- BEGIN EXAMPLE ---
 * @code

 int log_output_to_file(genptr_t data, genptr_t str)
 {
 FILE *fp = (FILE *)data;
 fprintf(fp, "LOG: %s", str);
 return 0;
 }

 int main(int ac, char *av[])
 {
 FILE *fp = fopen("whatever.log", "w+");
 bu_log_add_hook(log_output_to_file, (genptr_t)fp);
 bu_log("Logging to file.\n");
 bu_log_delete_hook(log_output_to_file, (genptr_t)fp);
 bu_log("Logging to stderr.\n");
 fclose(fp);
 return 0;
 }

 * @endcode
 * --- END EXAMPLE ---
 *
 */


/**
 * B U _ L O G _ I N D E N T _ D E L T A
 *
 * Change global indentation level by indicated number of characters.
 * Call with a large negative number to cancel all indentation.
 */
BU_EXPORT BU_EXTERN(void bu_log_indent_delta, (int delta));

/**
 * B U _ L O G _ I N D E N T _ V L S
 *
 * For multi-line vls generators, honor logindent level like bu_log() does,
 * and prefix the proper number of spaces.
 * Should be called at the front of each new line.
 */
BU_EXPORT BU_EXTERN(void bu_log_indent_vls, (struct bu_vls *v));

/**
 * B U _ L O G _ A D D _ H O O K
 *
 * Adds a hook to the list of bu_log hooks.  The top (newest) one of these
 * will be called with its associated client data and a string to be
 * processed.  Typcially, these hook functions will display the output
 * (possibly in an X window) or record it.
 *
 * NOTE: The hook functions are all non-PARALLEL.
 */
BU_EXPORT BU_EXTERN(void bu_log_add_hook, (bu_hook_t func, genptr_t clientdata));

/**
 * B U _ L O G _ D E L E T E _ H O O K
 *
 * Removes the hook matching the function and clientdata parameters from
 * the hook list.  Note that it is not necessarily the active (top) hook.
 */
BU_EXPORT BU_EXTERN(void bu_log_delete_hook, (bu_hook_t func, genptr_t clientdata));

/**
 * B U _ P U T C H A R
 *
 * Log a single character with no flushing.
 */
BU_EXPORT BU_EXTERN(void bu_putchar, (int c));

/**
 * B U _ L O G
 *
 * The routine is primarily called to log library events.
 */
BU_EXPORT BU_EXTERN(void bu_log, (const char *, ...)) __BU_ATTR_FORMAT12;

/**
 * B U _ F L O G
 *
 * Log a library event in the Standard way, to a specified file.
 */
BU_EXPORT BU_EXTERN(void bu_flog, (FILE *, const char *, ...)) __BU_ATTR_FORMAT23;

/** @} */

/** @addtogroup malloc */
/** @{ */
/** @file malloc.c
 *
 * @brief
 * Parallel-protected debugging-enhanced wrapper around system malloc().
 *
 * The bu_malloc() routines can't use bu_log() because that uses the
 * bu_vls() routines which depend on bu_malloc().  So it goes direct
 * to stderr, semaphore protected.
 *
 */

BU_EXPORT extern long bu_n_malloc;
BU_EXPORT extern long bu_n_free;
BU_EXPORT extern long bu_n_realloc;

/**
 * B U _ M A L L O C
 *
 * This routine only returns on successful allocation.  We promise
 * never to return a NULL pointer; caller doesn't have to check.
 * Allocation failure results in bu_bomb() being called.
 */
BU_EXPORT BU_EXTERN(genptr_t bu_malloc,
		    (size_t siz,
		     const char *str));

/**
 * B U _ C A L L O C
 *
 * This routine only returns on successful allocation.
 * We promise never to return a NULL pointer; caller doesn't have to check.
 * Failure results in bu_bomb() being called.
 */
BU_EXPORT BU_EXTERN(genptr_t bu_calloc,
		    (size_t nelem,
		     size_t elsize,
		     const char *str));

/**
 * B U _ F R E E
 *
 */
BU_EXPORT BU_EXTERN(void bu_free,
		    (genptr_t ptr,
		     const char *str));

/**
 * B U _ R E A L L O C
 *
 * bu_malloc()/bu_free() compatible wrapper for realloc().
 *
 * While the string 'str' is provided for the log messages, don't
 * disturb the mdb_str value, so that this storage allocation can be
 * tracked back to it's original creator.
 */
BU_EXPORT BU_EXTERN(genptr_t bu_realloc,
		    (genptr_t ptr,
		     size_t cnt,
		     const char *str));

/**
 * B U _ P R M E M
 *
 * Print map of memory currently in use.
 */
BU_EXPORT BU_EXTERN(void bu_prmem,
		    (const char *str));

/**
 * B U _ M A L L O C _ L E N _ R O U N D U P
 *
 * On systems with the CalTech malloc(), the amount of storage
 * ACTUALLY ALLOCATED is the amount requested rounded UP to the
 * nearest power of two.  For structures which are acquired and
 * released often, this works well, but for structures which will
 * remain unchanged for the duration of the program, this wastes as
 * much as 50% of the address space (and usually memory as well).
 * Here, we round up a byte size to the nearest power of two, leaving
 * off the malloc header, so as to ask for storage without wasting
 * any.
 *
 * On systems with the traditional malloc(), this strategy will just
 * consume the memory in somewhat larger chunks, but overall little
 * unused memory will be consumed.
 */
BU_EXPORT BU_EXTERN(int bu_malloc_len_roundup,
		    (int nbytes));

/**
 * B U _ C K _ M A L L O C _ P T R
 *
 * For a given pointer allocated by bu_malloc(), check the magic
 * number stored after the allocation area when BU_DEBUG_MEM_CHECK is
 * set.
 *
 * This is the individual version of bu_mem_barriercheck().
 *
 * returns if pointer good or BU_DEBUG_MEM_CHECK not set, bombs if
 * memory is corrupted.
 */
BU_EXPORT BU_EXTERN(void bu_ck_malloc_ptr,
		    (genptr_t ptr, const char *str));

/**
 * B U _ M E M _ B A R R I E R C H E C K
 *
 * Check *all* entries in the memory debug table for barrier word
 * corruption.  Intended to be called periodicly through an
 * application during debugging.  Has to run single-threaded, to
 * prevent table mutation.
 *
 * This is the bulk version of bu_ck_malloc_ptr()
 *
 * Returns -
 *  -1	something is wrong
 *   0	all is OK;
 */
BU_EXPORT BU_EXTERN(int bu_mem_barriercheck,
		    ());
/** @} */

/** @addtogroup bu_log */
/** @{ */
/** @file basename.c
 ** @file dirname.c
 *
 * @brief
 * Routines to process file and path names.
 *
 */

/**
 * B U _ D I R N A M E
 *
 * Given a filesystem pathname, return a pointer to a dynamic string
 * which is the parent directory of that file/directory.
 *
 *	/usr/dir/file	/usr/dir
 * @n	/usr/dir/	/usr
 * @n	/usr/file	/usr
 * @n	/usr/		/
 * @n	/usr		/
 * @n	/		/
 * @n	.		.
 * @n	..		.
 * @n	usr		.
 * @n	a/b		a
 * @n	a/		.
 * @n	../a/b		../a
 *
 * Warning: don't rely on non-constness of bu_dirname().. will change
 * to const.
 */
BU_EXPORT BU_EXTERN(char *bu_dirname,
		    (const char *cp));

/**
 * B U _ B A S E N A M E
 *
 * Given a string containing slashes such as a pathname, return a
 * pointer to the first character after the last slash.
 *
 *	/usr/dir/file	file
 * @n	/usr/dir/	dir
 * @n	/usr/		usr
 * @n	/usr		usr
 * @n	/		/
 * @n	.		.
 * @n	..		..
 * @n	usr		usr
 * @n	a/b		b
 * @n	a/		a
 */
BU_EXPORT BU_EXTERN(const char *bu_basename,
		    (const char *cp));

/** @} */

/** @addtogroup mf */
/** @{ */

/**
 * B U _ O P E N _ M A P P E D _ F I L E
 *
 * If the file can not be opened, as descriptive an error message as
 * possible will be printed, to simplify code handling in the caller.
 *
 * Mapped files are always opened read-only.
 *
 * If the system does not support mapped files, the data is read into
 * memory.
 */
BU_EXPORT BU_EXTERN(struct bu_mapped_file *bu_open_mapped_file,
		    (const char *name,
		     const char *appl));

/**
 * B U _ C L O S E _ M A P P E D _ F I L E
 *
 * Release a use of a mapped file.  Because it may be re-used shortly,
 * e.g. by the next frame of an animation, don't release the memory
 * even on final close, so that it's available when next needed.
 *
 * Call bu_free_mapped_files() after final close to reclaim space.
 * But only do that if you're SURE that ALL these files will never
 * again need to be mapped by this process.  Such as when running
 * multi-frame animations.
 */
BU_EXPORT BU_EXTERN(void bu_close_mapped_file,
		    (struct bu_mapped_file *mp));

/**
 * B U _ P R _ M A P P E D _ F I L E
 *
 */
BU_EXPORT BU_EXTERN(void bu_pr_mapped_file,
		    (const char *title,
		     const struct bu_mapped_file *mp));

/**
 * B U _ F R E E _ M A P P E D _ F I L E S
 *
 * Release storage being used by mapped files with no remaining users.
 * This entire routine runs inside a critical section, for parallel
 * protection.  Only call this routine if you're SURE that ALL these
 * files will never again need to be mapped by this process.  Such as
 * when running multi-frame animations.
 */
BU_EXPORT BU_EXTERN(void bu_free_mapped_files,
		    (int verbose));

/**
 * B U _ O P E N _ M A P P E D _ F I L E _ W I T H _ P A T H
 *
 * A wrapper for bu_open_mapped_file() which uses a search path to
 * locate the file.
 *
 * The search path is specified as a normal C argv array, terminated
 * by a null string pointer.  If the file name begins with a slash
 * ('/') the path is not used.
 */
BU_EXPORT BU_EXTERN(struct bu_mapped_file *bu_open_mapped_file_with_path,
		    (char * const *path,
		     const char *name,
		     const char *appl));


/** @} */

/** @addtogroup thread */
/** @{ */
/** @file kill.c
 *
 * terminate a given process.
 *
 */

/**
 * b u _ t e r m i n a t e
 *
 * terminate a given process.
 *
 * returns truthfully whether the process could be killed.
 */
BU_EXPORT BU_EXTERN(int bu_terminate, (int process));

/** @file process.c
 *
 * process management routines
 *
 */

/**
 * returns the process ID of the calling process
 */
BU_EXPORT BU_EXTERN(int bu_process_id, ());

/** @file parallel.c
 *
 * @brief routines for parallel processing
 *
 * Machine-specific routines for parallel processing.
 * Primarily calling functions in multiple threads on multiple CPUs.
 *
 */

/**
 * B U _ N I C E _ S E T
 *
 * Without knowing what the current UNIX "nice" value is, change to a
 * new absolute "nice" value.  (The system routine makes a relative
 * change).
 */
BU_EXPORT BU_EXTERN(void bu_nice_set, (int newnice));

/**
 * B U _ C P U L I M I T _ G E T
 *
 * Return the current CPU limit, in seconds. Zero or negative return
 * indicates that limits are not in effect.
 */
BU_EXPORT BU_EXTERN(int bu_cpulimit_get, ());

/**
 * B U _ C P U L I M I T _ S E T
 *
 * Set CPU time limit, in seconds.
 */
BU_EXPORT BU_EXTERN(void bu_cpulimit_set, (int sec));

/**
 * B U _ A V A I L _ C P U S
 *
 * Return the maximum number of physical CPUs that are considered to
 * be available to this process now.
 */
BU_EXPORT BU_EXTERN(int bu_avail_cpus, ());

/**
 * B U _ G E T _ L O A D _ A V E R A G E
 *
 * A generally portable method for obtaining the 1-minute load average.
 * Vendor-specific methods which don't involve a fork/exec sequence
 * would be preferable.
 * Alas, very very few systems put the load average in /proc,
 * most still grunge the avenrun[3] array out of /dev/kmem,
 * which requires special privleges to open.
 */
BU_EXPORT BU_EXTERN(fastf_t bu_get_load_average, ());

/**
 * B U _ G E T _ P U B L I C _ C P U S
 *
 * A general mechanism for non-privleged users of a server system to
 * control how many processors of their server get consumed by
 * multi-thread cruncher processes, by leaving a world-writable file.
 *
 * If the number in the file is negative, it means "all but that
 * many."
 *
 * Returns the number of processors presently available for "public"
 * use.
 *
 * DEPRECATED: this routine's use of a temporary file is deprecated
 * and should not be relied upon.  a future implementation will
 * utilize environment variables instead of temporary files.
 */
DEPRECATED BU_EXPORT BU_EXTERN(int bu_get_public_cpus, ());

/**
 * B U _ S E T _ R E A L T I M E
 *
 * If possible, mark this process for real-time scheduler priority.
 * Will often need root privs to succeed.
 *
 * Returns -
 * 1 realtime priority obtained
 * 0 running with non-realtime scheduler behavior
 */
BU_EXPORT BU_EXTERN(int bu_set_realtime, ());

/**
 * B U _ P A R A L L E L
 *
 * Create 'ncpu' copies of function 'func' all running in parallel,
 * with private stack areas.  Locking and work dispatching are handled
 * by 'func' using a "self-dispatching" paradigm.
 *
 * 'func' is called with one parameter, it's thread number.  Threads
 * are given increasing numbers, starting with zero.
 *
 * This function will not return control until all invocations of the
 * subroutine are finished.
 *
 * Don't use registers in this function (bu_parallel).  At least on
 * the Alliant, register context is NOT preserved when exiting the
 * parallel mode, because the serial portion resumes on some arbitrary
 * processor, not necessarily the one that serial execution started
 * on.  The registers are not shared.
 */
BU_EXPORT BU_EXTERN(void bu_parallel,
		    (void (*func)BU_ARGS((int ncpu, genptr_t arg)),
		     int ncpu,
		     genptr_t arg));

/** @} */

/** @addtogroup parse */
/** @{ */
/** @file ./libbu/parse.c
 *
 * @brief routines for parsing arbitrary structures
 *
 * Routines to assign values to elements of arbitrary structures.  The
 * layout of a structure to be processed is described by a structure
 * of type "bu_structparse", giving element names, element formats, an
 * offset from the beginning of the structure, and a pointer to an
 * optional "hooked" function that is called whenever that structure
 * element is changed.
 *
 * @par There are four basic operations supported:
 * @arg	print	struct elements to ASCII
 * @arg	parse	ASCII to struct elements
 * @arg	export	struct elements to machine-independent binary
 * @arg	import	machine-independent binary to struct elements
 *
 */

/**
 * B U _ S T R U C T _ E X P O R T
 *
 */
BU_EXPORT BU_EXTERN(int bu_struct_export,
		    (struct bu_external *ext,
		     const genptr_t base,
		     const struct bu_structparse *imp));

/**
 * B U _ S T R U C T _ I M P O R T
 *
 */
BU_EXPORT BU_EXTERN(int bu_struct_import,
		    (genptr_t base,
		     const struct bu_structparse *imp,
		     const struct bu_external *ext));

/**
 * B U _ S T R U C T _ P U T
 *
 * Put a structure in external form to a stdio file.  All formatting
 * must have been accomplished previously.
 */
BU_EXPORT BU_EXTERN(int bu_struct_put,
		    (FILE *fp,
		     const struct bu_external *ext));

/**
 * B U _ S T R U C T _ G E T
 *
 * Obtain the next structure in external form from a stdio file.
 */
BU_EXPORT BU_EXTERN(int bu_struct_get,
		    (struct bu_external *ext,
		     FILE *fp));

/**
 * B U _ S T R U C T _ W R A P _ B U F
 *
 * Given a buffer with an external representation of a structure
 * (e.g. the ext_buf portion of the output from bu_struct_export),
 * check it for damage in shipment, and if it's OK, wrap it up in an
 * bu_external structure, suitable for passing to bu_struct_import().
 */
BU_EXPORT BU_EXTERN(void bu_struct_wrap_buf,
		    (struct bu_external *ext,
		     genptr_t buf));

/**
 * B U _ S T R U C T P A R S E
 *
 * Parse the structure element description in the vls string "vls"
 * according to the structure description in "parsetab"
 *
 * @return <0 failure
 * @return  0 OK
 */
BU_EXPORT BU_EXTERN(int bu_struct_parse,
		    (const struct bu_vls *in_vls,
		     const struct bu_structparse *desc,
		     const char *base));

/**
 * B U _ S T R U C T P R I N T
 *
 */
BU_EXPORT BU_EXTERN(void bu_struct_print,
		    (const char *title,
		     const struct bu_structparse *parsetab,
		     const char *base));

/**
 * B U _ V L S _ S T R U C T P R I N T
 *
 * This differs from bu_struct_print in that this output is less
 * readable by humans, but easier to parse with the computer.
 */
BU_EXPORT BU_EXTERN(void bu_vls_struct_print,
		    (struct bu_vls *vls,
		     const struct bu_structparse *sdp,
		     const char *base));

/**
 * B U _ V L S _ S T R U C T P R I N T 2
 *
 * This differs from bu_struct_print in that it prints to a vls.
 */
BU_EXPORT BU_EXTERN(void bu_vls_struct_print2,
		    (struct bu_vls *vls,
		     const char *title,
		     const struct bu_structparse *sdp,
		     const char *base));

/**
 * b u _ v l s _ s t r u c t _ i t e m
 *
 * Convert a structure element (indicated by sdp) to its ASCII
 * representation in a VLS.
 */
BU_EXPORT BU_EXTERN(void bu_vls_struct_item,
		    (struct bu_vls *vp,
		     const struct bu_structparse *sdp,
		     const char *base,
		     int sep_char));

/**
 * B U _ V L S _ S T R U C T _ I T E M _ N A M E D
 *
 * Convert a structure element called "name" to an ASCII
 * representation in a VLS.
 */
BU_EXPORT BU_EXTERN(int bu_vls_struct_item_named,
		    (struct bu_vls *vp,
		     const struct bu_structparse *sdp,
		     const char *name,
		     const char *base,
		     int sep_char));

/**
 * b u _ p a r s e _ m m
 *
 * This allows us to specify the "size" parameter as values like ".5m"
 * or "27in" rather than using mm all the time.
 */
BU_EXPORT BU_EXTERN(void bu_parse_mm,
		    (const struct bu_structparse *sdp,
		     const char *name,
		     char *base,
		     const char *value));

/**
 *
 */
BU_EXPORT BU_EXTERN(int bu_key_eq_to_key_val,
		    (const char *in,
		     const char **next,
		     struct bu_vls *vls));

/**
 * B U _ S H A D E R _ T O _ T C L _ L I S T
 *
 * Take an old v4 shader specification of the form
 *
 * shadername arg1=value1 arg2=value2 color=1/2/3
 *
 * and convert it into the v5 Tcl-list form
 *
 * shadername {arg1 value1 arg2 value2 color 1/2/3}
 *
 * Note -- the input string is smashed with nulls.
 *
 * Note -- the v5 version is used everywhere internally, and in v5
 * databases.
 *
 *
 * @return 1 error
 * @return 0 OK
 */
BU_EXPORT BU_EXTERN(int bu_shader_to_tcl_list,
		    (const char *in,
		     struct bu_vls *vls));

/**
 * B U _ S H A D E R _ T O _ K E Y _ E Q
 *
 */
BU_EXPORT BU_EXTERN(int bu_shader_to_key_eq,
		    (const char *in, struct bu_vls *vls));

/**
 * B U _ F W R I T E _ E X T E R N A L
 *
 * Take a block of memory, and write it into a file.
 *
 * Caller is responsible for freeing memory of external representation,
 * using bu_free_external().
 *
 * @return <0 error
 * @return  0 OK
 */
BU_EXPORT BU_EXTERN(int bu_fwrite_external,
		    (FILE *fp,
		     const struct bu_external *ep));

/**
 * B U _ H E X D U M P _ E X T E R N A L
 *
 */
BU_EXPORT BU_EXTERN(void bu_hexdump_external,
		    (FILE *fp, const struct bu_external *ep,
		     const char *str));

/**
 * B U _ F R E E _ E X T E R N A L
 *
 */
BU_EXPORT BU_EXTERN(void bu_free_external,
		    (struct bu_external *ep));

/**
 * B U _ C O P Y _ E X T E R N A L
 *
 */
BU_EXPORT BU_EXTERN(void bu_copy_external,
		    (struct bu_external *op,
		     const struct bu_external *ip));

/**
 * B U _ N E X T _ T O K E N
 *
 * Advance pointer through string over current token,
 * across white space, to beginning of next token.
 */
BU_EXPORT BU_EXTERN(char *bu_next_token,
		    (char *str));

/**
 *
 */
BU_EXPORT BU_EXTERN(void bu_structparse_get_terse_form,
		    (struct bu_vls *logstr,
		     const struct bu_structparse *sp));

/**
 * b u _ s t r u c t p a r s e _ a r g v
 *
 * Support routine for db adjust and db put.  Much like the
 * bu_struct_parse routine which takes its input as a bu_vls. This
 * routine, however, takes the arguments as lists, a more Tcl-friendly
 * method. There is a log vls for storing messages.
 *
 * Operates on argv[0] and argv[1], then on argv[2] and argv[3], ...
 *
 *
 * @param log	- vls for dumping info that might have gone to bu_log
 * @param argc	- number of elements in argv
 * @param argv	- contains the keyword-value pairs
 * @param desc	- structure description
 * @param base	- base addr of users struct
 *
 * @retval TCL_OK if successful,
 * @retval TCL_ERROR on failure
 */
BU_EXPORT BU_EXTERN(int bu_structparse_argv,
		    (struct bu_vls *logstr,
		     int argc,
		     char **argv,
		     const struct bu_structparse *desc,
		     char *base));

/**
 * b u _ s p _ s k i p _ s e p
 *
 * Skip the separator(s) (i.e. whitespace and open-braces)
 *
 * @param _cp	- character pointer
 */
#define BU_SP_SKIP_SEP(_cp) { \
	while (*(_cp) && (*(_cp) == ' ' || *(_cp) == '\n' || \
	*(_cp) == '\t' || *(_cp) == '{'))  ++(_cp); \
}


/** @} */
/** @addtogroup bitv */
/** @{ */
/** @file printb.c
 *
 * @brief print bitfields
 *
 */

/**
 * B U _ V L S _ P R I N T B
 *
 * Format a value a la the %b format of the kernel's printf
 *
 * @param   vls	variable length string to put output in
 * @param    s		title string
 * @param   v		the integer with the bits in it
 * @param   bits	a string which starts with the desired base (8 or 16)
 * as \\010 or \\020, followed by
 * words preceeded with embedded low-value bytes indicating
 * bit number plus one,
 * in little-endian order, eg:
 * "\010\2Bit_one\1BIT_zero"
 */
BU_EXPORT BU_EXTERN(void bu_vls_printb,
		    (struct bu_vls *vls,
		     const char *s, unsigned long v,
		     const char *bits));

/**
 * B U _ P R I N T B
 *
 * Format and print, like bu_vls_printb().
 */
BU_EXPORT BU_EXTERN(void bu_printb,
		    (const char *s,
		     unsigned long v,
		     const char *bits));

/** @} */

/** @addtogroup ptbl */
/** @{ */

/**
 * B U _ P T B L _ I N I T
 *
 * Initialize struct & get storage for table.
 * Recommend 8 or 64 for initial len.
 */
BU_EXPORT BU_EXTERN(void bu_ptbl_init,
		    (struct bu_ptbl *b,
		     size_t len,
		     const char *str));

/**
 * B U _ P T B L _ R E S E T
 *
 * Reset the table to have no elements, but retain any existing
 * storage.
 */
BU_EXPORT BU_EXTERN(void bu_ptbl_reset,
		    (struct bu_ptbl *b));

/**
 * B U _ P T B L _ I N S
 *
 * Append/Insert a (long *) item to/into the table.
 */
BU_EXPORT BU_EXTERN(int bu_ptbl_ins,
		    (struct bu_ptbl *b,
		     long *p));

/**
 * B U _ P T B L _ L O C A T E
 *
 * locate a (long *) in an existing table
 *
 *
 * @return index of first matching element in array, if found
 * @return -1 if not found
 *
 * We do this a great deal, so make it go as fast as possible.  this
 * is the biggest argument I can make for changing to an ordered list.
 * Someday....
 */
BU_EXPORT BU_EXTERN(int bu_ptbl_locate,
		    (const struct bu_ptbl *b,
		     const long *p));

/**
 * B U _ P T B L _ Z E R O
 *
 * Set all occurrences of "p" in the table to zero.  This is different
 * than deleting them.
 */
BU_EXPORT BU_EXTERN(void bu_ptbl_zero,
		    (struct bu_ptbl *b,
		     const long *p));

/**
 * B U _ P T B L _ I N S _ U N I Q U E
 *
 * Append item to table, if not already present.  Unique insert.
 *
 * @return index of first matching element in array, if found.  (table unchanged)
 * @return -1 if table extended to hold new element
 *
 * We do this a great deal, so make it go as fast as possible.  this
 * is the biggest argument I can make for changing to an ordered list.
 * Someday....
 */
BU_EXPORT BU_EXTERN(int bu_ptbl_ins_unique,
		    (struct bu_ptbl *b, long *p));

/**
 * B U _ P T B L _ R M
 *
 * Remove all occurrences of an item from a table
 *
 * @return Number of copies of 'p' that were removed from the table.
 * @return 0 if none found.
 *
 * we go backwards down the table looking for occurrences of p to
 * delete.  We do it backwards to reduce the amount of data moved when
 * there is more than one occurrence of p in the table.  A pittance
 * savings, unless you're doing a lot of it.
 */
BU_EXPORT BU_EXTERN(int bu_ptbl_rm,
		    (struct bu_ptbl *b,
		     const long *p));

/**
 * B U _ P T B L _ C A T
 *
 * Catenate one table onto end of another.  There is no checking for
 * duplication.
 */
BU_EXPORT BU_EXTERN(void bu_ptbl_cat,
		    (struct bu_ptbl *dest,
		     const struct bu_ptbl *src));

/**
 * B U _ P T B L _ C A T _ U N I Q
 *
 * Catenate one table onto end of another, ensuring that no entry is
 * duplicated.  Duplications between multiple items in 'src' are not
 * caught.  The search is a nasty n**2 one.  The tables are expected
 * to be short.
 */
BU_EXPORT BU_EXTERN(void bu_ptbl_cat_uniq,
		    (struct bu_ptbl *dest,
		     const struct bu_ptbl *src));

/**
 * B U _ P T B L _ F R E E
 *
 * Deallocate dynamic buffer associated with a table, and render this
 * table unusable without a subsequent bu_ptbl_init().
 */
BU_EXPORT BU_EXTERN(void bu_ptbl_free,
		    (struct bu_ptbl *b));

/**
 * B U _ P T B L
 *
 * This version maintained for source compatibility with existing NMG
 * code.
 */
BU_EXPORT BU_EXTERN(int bu_ptbl,
		    (struct bu_ptbl *b,
		     int func, long *p));

/**
 * B U _ P R _ P T B L
 *
 * Print a bu_ptbl array for inspection.
 */
BU_EXPORT BU_EXTERN(void bu_pr_ptbl,
		    (const char *title,
		     const struct bu_ptbl *tbl,
		     int verbose));

/**
 * B U _ P T B L _ T R U N C
 *
 * truncate a bu_ptbl
 */
BU_EXPORT BU_EXTERN(void bu_ptbl_trunc,
		    (struct bu_ptbl *tbl,
		     int end));

/** @} */

/** @addtogroup rb */
/** @{ */
/** @file rb_create.c
 *
 * Routines to create a red-black tree
 *
 */

/**
 * B U _ R B _ C R E A T E
 *
 * Create a red-black tree
 *
 * This function has three parameters: a comment describing the tree
 * to create, the number of linear orders to maintain simultaneously,
 * and the comparison functions (one per order).  bu_rb_create()
 * returns a pointer to the red-black tree header record.
 */
BU_EXPORT BU_EXTERN(bu_rb_tree *bu_rb_create,
		    (char *description,
		     int nm_orders,
		     int (**order_funcs)()));

/**
 * B U _ R B _ C R E A T E 1
 *
 * Create a single-order red-black tree
 *
 * This function has two parameters: a comment describing the tree to
 * create and a comparison function.  bu_rb_create1() builds an array
 * of one function pointer and passes it to bu_rb_create().
 * bu_rb_create1() returns a pointer to the red-black tree header
 * record.
 *
 * N.B. - Since this function allocates storage for the array of
 * function pointers, in order to avoid memory leaks on freeing the
 * tree, applications should call bu_rb_free1(), NOT bu_rb_free().
 */
BU_EXPORT BU_EXTERN(bu_rb_tree *bu_rb_create1,
		    (char *description,
		     int (*order_func)()));

/** @file rb_delete.c
 *
 * Routines to delete a node from a red-black tree
 *
 */

/**
 * B U _ R B _ D E L E T E
 *
 * Applications interface to _rb_delete()
 *
 * This function has two parameters: the tree and order from which to
 * do the deleting.  bu_rb_delete() removes the data block stored in
 * the current node (in the position of the specified order) from
 * every order in the tree.
 */
BU_EXPORT BU_EXTERN(void bu_rb_delete,
		    (bu_rb_tree *tree,
		     int order));
#define bu_rb_delete1(t)	bu_rb_delete((t), 0)

/** @file rb_diag.c
 *
 * Diagnostic routines for red-black tree maintenance
 *
 */

/**
 * B U _ R B _ D I A G N O S E _ T R E E
 *
 * Produce a diagnostic printout of a red-black tree
 *
 * This function has three parameters: the root and order of the tree
 * to print out and the type of traversal (preorder, inorder, or
 * postorder).
 */
BU_EXPORT BU_EXTERN(void bu_rb_diagnose_tree,
		    (bu_rb_tree *tree,
		     int order,
		     int trav_type));

/**
 * B U _ R B _ S U M M A R I Z E _ T R E E
 *
 * Describe a red-black tree
 *
 * This function has one parameter: a pointer to a red-black tree.
 * bu_rb_summarize_tree() prints out the header information for the
 * tree.  It is intended for diagnostic purposes.
 */
BU_EXPORT BU_EXTERN(void bu_rb_summarize_tree,
		    (bu_rb_tree *tree));

/** @file rb_extreme.c
 *
 * Routines to extract mins, maxes, adjacent, and current nodes
 * from a red-black tree
 *
 */


/**
 * B U _ R B _ E X T R E M E
 *
 * Applications interface to rb_extreme()
 *
 * This function has three parameters: the tree in which to find an
 * extreme node, the order on which to do the search, and the sense
 * (min or max).  On success, bu_rb_extreme() returns a pointer to the
 * data in the extreme node.  Otherwise it returns NULL.
 */
BU_EXPORT BU_EXTERN(void *bu_rb_extreme,
		    (bu_rb_tree *tree,
		     int order,
		     int sense));

/**
 * B U _ R B _ N E I G H B O R
 *
 * Return a node adjacent to the current red-black node
 *
 * This function has three parameters: the tree and order on which to
 * do the search and the sense (min or max, which is to say
 * predecessor or successor) of the search.  bu_rb_neighbor() returns
 * a pointer to the data in the node adjacent to the current node in
 * the specified direction, if that node exists.  Otherwise, it
 * returns NULL.
 */
BU_EXPORT BU_EXTERN(void *bu_rb_neighbor,
		    (bu_rb_tree *tree,
		     int order,
		     int sense));

/**
 * B U _ R B _ C U R R
 *
 * Return the current red-black node
 *
 * This function has two parameters: the tree and order in which to
 * find the current node.  bu_rb_curr() returns a pointer to the data
 * in the current node, if it exists.  Otherwise, it returns NULL.
 */
BU_EXPORT BU_EXTERN(void *bu_rb_curr,
		    (bu_rb_tree *tree,
		     int order));
#define bu_rb_curr1(t)	bu_rb_curr((t), 0)

/** @file rb_free.c
 *
 * Routines to free a red-black tree
 *
 */

/**
 * B U _ R B _ F R E E
 *
 * Free a red-black tree
 *
 * This function has two parameters: the tree to free and a function
 * to handle the application data.  bu_rb_free() traverses tree's
 * lists of nodes and packages, freeing each one in turn, and then
 * frees tree itself.  If free_data is non-NULL, then bu_rb_free()
 * calls it just* before freeing each package, passing it the
 * package's rbp_data member.  Otherwise, the application data is left
 * untouched.
 */
BU_EXPORT BU_EXTERN(void bu_rb_free,
		    (bu_rb_tree *tree,
		     void (*free_data)()));
#define BU_RB_RETAIN_DATA ((void (*)()) 0)
#define bu_rb_free1(t, f)					\
		{							\
		    BU_CKMAG((t), BU_RB_TREE_MAGIC, "red-black tree");	\
		    bu_free((char *) ((t) -> rbt_order),		\
				"red-black order function");		\
		    bu_rb_free(t, f);					\
		}

/** @file rb_insert.c
 *
 * Routines to insert into a red-black tree
 *
 */

/**
 * B U _ R B _ I N S E R T
 *
 * Applications interface to _rb_insert()
 *
 * This function has two parameters: the tree into which to insert the
 * new node and the contents of the node.  If a uniqueness requirement
 * would be violated, bu_rb_insert() does nothing but return a number
 * from the set {-1, -2, ..., -nm_orders} of which the absolute value
 * is the first order for which a violation exists.  Otherwise, it
 * returns the number of orders for which the new node was equal to a
 * node already in the tree.
 */
BU_EXPORT BU_EXTERN(int bu_rb_insert,
		    (bu_rb_tree *tree,
		     void *data));

/**
 * B U _ R B _ I S _ U N I Q
 *
 * Query the uniqueness flag for one order of a red-black tree
 *
 * This function has two parameters: the tree and the order for which
 * to query uniqueness.
 */
BU_EXPORT BU_EXTERN(int bu_rb_is_uniq,
		    (bu_rb_tree *tree,
		     int order));
#define bu_rb_is_uniq1(t)	bu_rb_is_uniq((t), 0)

/**
 * B U _ R B _ S E T _ U N I Q V
 *
 * Set the uniqueness flags for all the linear orders of a red-black
 * tree
 *
 * This function has two parameters: the tree and a bitv_t encoding
 * the flag values.  bu_rb_set_uniqv() sets the flags according to the
 * bits in flag_rep.  For example, if flag_rep = 1011_2, then the
 * first, second, and fourth orders are specified unique, and the
 * third is specified not-necessarily unique.
 */
BU_EXPORT BU_EXTERN(void bu_rb_set_uniqv,
		    (bu_rb_tree *tree,
		     bitv_t vec));

/**
 * B U _ R B _ U N I Q _ A L L _ O F F
 *
 * These functions have one parameter: the tree for which to
 * require uniqueness/permit nonuniqueness.
 */
BU_EXPORT BU_EXTERN(void bu_rb_uniq_all_off,
		    (bu_rb_tree *tree));

/**
 * B U _ R B _ U N I Q _ A L L _ O N
 *
 * These functions have one parameter: the tree for which to
 * require uniqueness/permit nonuniqueness.
 */
BU_EXPORT BU_EXTERN(void bu_rb_uniq_all_on,
		    (bu_rb_tree *tree));

/**
 * B U _ R B _ U N I Q _ O N
 *
 * Has two parameters: the tree and the order for which to require
 * uniqueness/permit nonuniqueness.  Each sets the specified flag to
 * the specified value and returns the previous value of the flag.
 */
BU_EXPORT BU_EXTERN(int bu_rb_uniq_on,
		    (bu_rb_tree *tree,
		     int order));
#define bu_rb_uniq_on1(t)	bu_rb_uniq_on((t), 0)

/**
 * B U _ R B _ U N I Q _ O F F
 *
 * Has two parameters: the tree and the order for which to require
 * uniqueness/permit nonuniqueness.  Each sets the specified flag to
 * the specified value and returns the previous value of the flag.
 */
BU_EXPORT BU_EXTERN(int bu_rb_uniq_off,
		    (bu_rb_tree *tree,
		     int order));
#define bu_rb_uniq_off1(t)	bu_rb_uniq_off((t), 0)

/** @file rb_order_stats.c
 *
 * Routines to support order-statistic operations for a red-black tree
 *
 */

/**
 * B U _ R B _ R A N K
 *
 * Determines the rank of a node in one order of a red-black tree
 *
 * This function has two parameters: the tree in which to search and
 * the order on which to do the searching.  If the current node is
 * null, bu_rb_rank() returns 0.  Otherwise, it returns the rank of
 * the current node in the specified order.  bu_rb_rank() is an
 * implementation of the routine OS-RANK on p. 283 of Cormen et al.
 */
BU_EXPORT BU_EXTERN(int bu_rb_rank,
		    (bu_rb_tree *tree,
		     int order));
#define bu_rb_rank1(t)	bu_rb_rank1((t), 0)

/**
 * B U _ R B _ S E L E C T
 *
 * This function has three parameters: the tree in which to search,
 * the order on which to do the searching, and the rank of interest.
 * On success, bu_rb_select() returns a pointer to the data block in
 * the discovered node.  Otherwise, it returns NULL.
 */
BU_EXPORT BU_EXTERN(void *bu_rb_select,
		    (bu_rb_tree *tree,
		     int order,
		     int k));
#define bu_rb_select1(t, k)	bu_rb_select((t), 0, (k))

/** @file rb_search.c
 *
 * Routines to search for a node in a red-black tree
 *
 */

/**
 * B U _ R B _ S E A R C H
 *
 * This function has three parameters: the tree in which to search,
 * the order on which to do the searching, and a data block containing
 * the desired value of the key.  On success, bu_rb_search() returns a
 * pointer to the data block in the discovered node.  Otherwise, it
 * returns NULL.
 */
BU_EXPORT BU_EXTERN(void *bu_rb_search,
		    (bu_rb_tree *tree,
		     int order,
		     void *data));
#define bu_rb_search1(t, d)	bu_rb_search((t), 0, (d))

/** @file rb_walk.c
 *
 * Routines for traversal of red-black trees
 *
 * The function burb_walk() is defined in terms of the function
 * rb_walk(), which, in turn, calls any of the six functions
 *
 * @arg		- static void prewalknodes()
 * @arg		- static void inwalknodes()
 * @arg		- static void postwalknodes()
 * @arg		- static void prewalkdata()
 * @arg		- static void inwalkdata()
 * @arg		- static void postwalkdata()
 *
 * depending on the type of traversal desired and the objects
 * to be visited (nodes themselves, or merely the data stored
 * in them).  Each of these last six functions has four parameters:
 * the root of the tree to traverse, the order on which to do the
 * walking, the function to apply at each visit, and the current
 * depth in the tree.
 */

/**
 * B U _ R B _ W A L K ()
 *
 * This function has four parameters: the tree to traverse, the order
 * on which to do the walking, the function to apply to each node, and
 * the type of traversal (preorder, inorder, or postorder).
 */
BU_EXPORT BU_EXTERN(void bu_rb_walk,
		    (bu_rb_tree *tree,
		     int order,
		     void (*visit)(),
		     int trav_type));
#define bu_rb_walk1(t, v, d)	bu_rb_walk((t), 0, (v), (d))

/** @} */
/** @addtogroup thread */
/** @{ */

/** @file semaphore.c
 *
 * @brief semaphore implementation
 *
 * Machine-specific routines for parallel processing.  Primarily for
 * handling semaphores for critical sections.
 *
 * The new paradigm: semaphores are referred to, not by a pointer, but
 * by a small integer.  This module is now responsible for obtaining
 * whatever storage is needed to implement each semaphore.
 *
 * Note that these routines can't use bu_log() for error logging,
 * because bu_log() accquires semaphore #0 (BU_SEM_SYSCALL).
 */

/**
 * B U _ S E M A P H O R E _ I N I T
 *
 * Prepare 'nsemaphores' independent critical section semaphores.  Die
 * on error.
 *
 * Takes the place of 'n' separate calls to old RES_INIT().  Start by
 * allocating array of "struct bu_semaphores", which has been arranged
 * to contain whatever this system needs.
 *
 */
BU_EXPORT BU_EXTERN(void bu_semaphore_init,
		    (unsigned int nsemaphores));

/**
 * B U _ S E M A P H O R E _ R E I N I T
 *
 * Prepare 'nsemaphores' independent critical section semaphores.  Die
 * on error.
 */
BU_EXPORT BU_EXTERN(void bu_semaphore_reinit,
		    (unsigned int nsemaphores));

/**
 * B U _ S E M A P H O R E _ A C Q U I R E
 *
 */
BU_EXPORT BU_EXTERN(void bu_semaphore_acquire,
		    (unsigned int i));

/**
 * B U _ S E M A P H O R E _ R E L E A S E
 *
 */
BU_EXPORT BU_EXTERN(void bu_semaphore_release,
		    (unsigned int i));

/** @} */
/** @addtogroup vls */
/** @{ */

/**
 * b u _ v l s _ i n i t
 *
 * No storage should be allocated at this point, and bu_vls_addr()
 * must be able to live with that.
 */
BU_EXPORT BU_EXTERN(void bu_vls_init,
		    (struct bu_vls *vp));

/**
 * b u _ v l s _ i n i t _ i f _ u n i n i t
 *
 * If a VLS is unitialized, initialize it.  If it is already
 * initialized, leave it alone, caller wants to append to it.
 */
BU_EXPORT BU_EXTERN(void bu_vls_init_if_uninit,
		    (struct bu_vls *vp));

/**
 * b u _ v l s _ v l s i n i t
 *
 * Allocate storage for a struct bu_vls, call bu_vls_init on it, and
 * return the result.  Allows for creation of dynamically allocated
 * VLS strings.
 */
BU_EXPORT BU_EXTERN(struct bu_vls *bu_vls_vlsinit,
		    ());

/**
 * b u _ v l s _ a d d r
 *
 * Return a pointer to the null-terminated string in the vls array.
 * If no storage has been allocated yet, give back a valid string.
 */
BU_EXPORT BU_EXTERN(char *bu_vls_addr,
		    (const struct bu_vls *vp));

/**
 * b u _ v l s _ e x t e n d
 *
 * Ensure that the provided VLS has at least 'extra' characters of
 * space available.  Additional space is allocated in minimum step
 * sized amounts and may allocate more than requested.
 */
BU_EXPORT BU_EXTERN(void bu_vls_extend,
		    (struct bu_vls *vp,
		     unsigned int extra));

/**
 * b u _ v l s _ s e t l e n
 *
 * Ensure that the vls has a length of at least 'newlen', and make
 * that the current length.
 *
 * Useful for subroutines that are planning on mucking with the data
 * array themselves.  Not advisable, but occasionally useful.
 *
 * Does not change the offset from the front of the buffer, if any.
 * Does not initialize the value of any of the new bytes.
 */
BU_EXPORT BU_EXTERN(void bu_vls_setlen,
		    (struct bu_vls *vp,
		     int newlen));
/**
 * b u _ v l s _ s t r l e n
 *
 * Return length of the string, in bytes, not including the null
 * terminator.
 */
BU_EXPORT BU_EXTERN(int bu_vls_strlen,
		    (const struct bu_vls *vp));

/**
 * b u _ v l s _ t r u n c
 *
 * Truncate string to at most 'len' characters.  If 'len' is negative,
 * trim off that many from the end.  If 'len' is zero, don't release
 * storage -- user is probably just going to refill it again,
 * e.g. with bu_vls_gets().
 */
BU_EXPORT BU_EXTERN(void bu_vls_trunc,
		    (struct bu_vls *vp,
		     int len));

/**
 * b u _ v l s _ t r u n c 2
 *
 * Son of bu_vls_trunc().  Same as bu_vls_trunc() except that it
 * doesn't truncate (or do anything) if the len is negative.
 */
BU_EXPORT BU_EXTERN(void bu_vls_trunc2,
		    (struct bu_vls *vp,
		     int len));

/**
 * b u _ v l s _ n i b b l e
 *
 * "Nibble" 'len' characters off the front of the string.  Changes the
 * length and offset; no data is copied.
 *
 * 'len' may be positive or negative. If negative, characters are
 * un-nibbled.
 */
BU_EXPORT BU_EXTERN(void bu_vls_nibble,
		    (struct bu_vls *vp,
		     int len));

/**
 * b u _ v l s _ f r e e
 *
 * Releases the memory used for the string buffer.
 */
BU_EXPORT BU_EXTERN(void bu_vls_free,
		    (struct bu_vls *vp));

/**
 * b u _ v l s _ v l s f r e e
 *
 * Releases the memory used for the string buffer and the memory for
 * the vls structure
 */
BU_EXPORT BU_EXTERN(void bu_vls_vlsfree,
		    (struct bu_vls *vp));
/**
 * b u _ v l s _ s t r d u p
 *
 * Make an "ordinary" string copy of a vls string.  Storage for the
 * regular string is acquired using malloc.
 *
 * The source string is not affected.
 */
BU_EXPORT BU_EXTERN(char *bu_vls_strdup,
		    (const struct bu_vls *vp));

/**
 * b u _ v l s _ s t r g r a b
 *
 * Like bu_vls_strdup(), but destructively grab the string from the
 * source argument 'vp'.  This is more efficient than bu_vls_strdup()
 * for those instances where the source argument 'vp' is no longer
 * needed by the caller, as it avoides a potentially long buffer copy.
 *
 * The source string is destroyed, as if bu_vls_free() had been
 * called.
 */
BU_EXPORT BU_EXTERN(char *bu_vls_strgrab,
		    (struct bu_vls *vp));

/**
 * b u _ v l s _ s t r c p y
 *
 * Empty the vls string, and copy in a regular string.
 */
BU_EXPORT BU_EXTERN(void bu_vls_strcpy,
		    (struct bu_vls *vp,
		     const char *s));

/**
 * b u _ v l s _ s t r n c p y
 *
 * Empty the vls string, and copy in a regular string, up to N bytes
 * long.
 */
BU_EXPORT BU_EXTERN(void bu_vls_strncpy,
		    (struct bu_vls *vp,
		     const char *s,
		     size_t n));

/**
 * b u _ v l s _ s t r c a t
 *
 * Concatenate a new string onto the end of the existing vls string.
 */
BU_EXPORT BU_EXTERN(void bu_vls_strcat,
		    (struct bu_vls *vp,
		     const char *s));

/**
 * b u _ v l s _ s t r n c a t
 *
 * Concatenate a new string onto the end of the existing vls string.
 */
BU_EXPORT BU_EXTERN(void bu_vls_strncat,
		    (struct bu_vls *vp,
		     const char *s,
		     size_t n));

/**
 * b u _ v l s _ v l s c a t
 *
 * Concatenate a new vls string onto the end of an existing vls
 * string.  The storage of the source string is not affected.
 */
BU_EXPORT BU_EXTERN(void bu_vls_vlscat,
		    (struct bu_vls *dest,
		     const struct bu_vls *src));

/**
 * b u _ v l s _ v l s c a t z a p
 *
 * Concatenate a new vls string onto the end of an existing vls
 * string.  The storage of the source string is released (zapped).
 */
BU_EXPORT BU_EXTERN(void bu_vls_vlscatzap,
		    (struct bu_vls *dest,
		     struct bu_vls *src));

/**
 * b u _ v l s _ s t r c m p
 *
 * Lexicographically compare two vls strings.  Returns an integer
 * greater than, equal to, or less than 0, according as the string s1
 * is greater than, equal to, or less than the string s2.
 */
BU_EXPORT BU_EXTERN(int bu_vls_strcmp,
		    (struct bu_vls *s1,
		     struct bu_vls *s2));

/**
 * b u _ v l s _ s t r n c m p
 *
 * Lexicographically compare two vls strings up to n characters.
 * Returns an integer greater than, equal to, or less than 0,
 * according as the string s1 is greater than, equal to, or less than
 * the string s2.
 */
BU_EXPORT BU_EXTERN(int bu_vls_strncmp,
		    (struct bu_vls *s1,
		     struct bu_vls *s2,
		     size_t n));

/**
 * b u _ v l s _ f r o m _ a r g v
 *
 * Given and argc & argv pair, convert them into a vls string of
 * space-separated words.
 */
BU_EXPORT BU_EXTERN(void bu_vls_from_argv,
		    (struct bu_vls *vp,
		     int argc,
		     const char *argv[]));

/**
 * b u _ a r g v _ f r o m _ s t r i n g
 *
 * Build argv[] array from input buffer, by splitting whitespace
 * separated "words" into null terminated strings.
 *
 * 'lim' indicates the maximum number of elements that can be stored
 * in the argv[] array not including a terminating NULL.
 *
 * The input buffer is altered by this process.  The argv[] array
 * points into the input buffer.  The argv[] array needs to have at
 * least lim+1 pointers allocated for lim items plus a terminating
 * pointer to NULL.  The input buffer should not be freed until argv
 * has been freed or passes out of scope.
 *
 * Returns -
 * 0	no words in input
 * argc	number of words of input, now in argv[]
 */
BU_EXPORT BU_EXTERN(int bu_argv_from_string,
		    (char *argv[],
		     int lim,
		     char *lp));

/**
 * b u _ v l s _ f w r i t e
 *
 * Write the VLS to the provided file pointer.
 */
BU_EXPORT BU_EXTERN(void bu_vls_fwrite,
		    (FILE *fp,
		     const struct bu_vls *vp));

/**
 * b u _ v l s _ w r i t e
 *
 * Write the VLS to the provided file descriptor.
 */
BU_EXPORT BU_EXTERN(void bu_vls_write,
		    (int fd,
		     const struct bu_vls *vp));

/**
 * b u _ v l s _ r e a d
 *
 * Read the remainder of a UNIX file onto the end of a vls.
 *
 * Returns -
 * nread number of characters read
 *  0 if EOF encountered immediately
 * -1 read error
 */
BU_EXPORT BU_EXTERN(int bu_vls_read,
		    (struct bu_vls *vp,
		     int fd));

/**
 * b u _ v l s _ g e t s
 *
 * Append a newline-terminated string from the file pointed to by "fp"
 * to the end of the vls pointed to by "vp".  The newline from the
 * file is read, but not stored into the vls.
 *
 * The most common error is to forget to bu_vls_trunc(vp, 0) before
 * reading the next line into the vls.
 *
 * Returns -
 *   >=0  the length of the resulting vls
 *   -1   on EOF where no characters were read or added to the vls
 */
BU_EXPORT BU_EXTERN(int bu_vls_gets,
		    (struct bu_vls *vp,
		     FILE *fp));

/**
 * b u _ v l s _ p u t c
 *
 * Append the given character to the vls.
 */
BU_EXPORT BU_EXTERN(void bu_vls_putc,
		    (struct bu_vls *vp,
		     int c));

/**
 * b u _ v l s _ t r i m s p a c e
 *
 * Remove leading and trailing white space from a vls string.
 */
BU_EXPORT BU_EXTERN(void bu_vls_trimspace,
		    (struct bu_vls *vp));

/**
 * b u _ v l s _ v p r i n t f
 *
 * Format a string into a vls.  This version should work on
 * practically any machine, but it serves to highlight the the
 * grossness of the varargs package requiring the size of a parameter
 * to be known at compile time.
 *
 * %s continues to be a regular 'C' string, null terminated.
 * %V is a pointer to a (struct bu_vls *) string.
 *
 * This routine appends to the given vls similar to how vprintf
 * appends to stdout (see bu_vls_vsprintf for overwriting the vls).
 */
BU_EXPORT BU_EXTERN(void bu_vls_vprintf,
		    (struct bu_vls *vls,
		     const char *fmt,
		     va_list ap));

/**
 * b u _ v l s _ p r i n t f
 *
 * Initializes the va_list, then calls the above bu_vls_vprintf.
 */
BU_EXPORT BU_EXTERN(void bu_vls_printf,
		    (struct bu_vls *vls,
		     const char *fmt, ...)) __BU_ATTR_FORMAT23;

/**
 * b u _ v l s _ s p r i n t f
 *
 * Format a string into a vls, setting the vls to the given print
 * specifier expansion.  This routine truncates any existing vls
 * contents beforehand (i.e. it doesn't append, see bu_vls_vprintf for
 * appending to the vls).
 *
 * %s continues to be a regular 'C' string, null terminated.
 * %V is a pointer to a (struct bu_vls *) string.
 */
BU_EXPORT BU_EXTERN(void bu_vls_sprintf,
		    (struct bu_vls *vls,
		     const char *fmt, ...)) __BU_ATTR_FORMAT23;

/**
 * b u _ v l s _ s p a c e s
 *
 * Efficiently append 'cnt' spaces to the current vls.
 */
BU_EXPORT BU_EXTERN(void bu_vls_spaces,
		    (struct bu_vls *vp,
		     int cnt));

/**
 * b u _ v l s _ p r i n t _ p o s i t i o n s _ u s e d
 *
 * Returns number of printed spaces used on final output line of a
 * potentially multi-line vls.  Useful for making decisions on when to
 * line-wrap.
 *
 * Accounts for normal UNIX tab-expansion:
 *	         1         2         3         4
 *	1234567890123456789012345678901234567890
 *	        x       x       x       x
 *
 *	0-7 --> 8, 8-15 --> 16, 16-23 --> 24, etc.
 */
BU_EXPORT BU_EXTERN(int bu_vls_print_positions_used,
		    (const struct bu_vls *vp));

/**
 * b u _ v l s _ d e t a b
 *
 * Given a vls, return a version of that string which has had all
 * "tab" characters converted to the appropriate number of spaces
 * according to the UNIX tab convention.
 */
BU_EXPORT BU_EXTERN(void bu_vls_detab,
		    (struct bu_vls *vp));

/**
 * b u _ v l s _ p r e p e n d
 *
 * Add a string to the begining of the vls.
 */
BU_EXPORT BU_EXTERN(void bu_vls_prepend,
		    (struct bu_vls *vp,
		     char *str));

/**
 * b u _ v l s _ e n c o d e
 *
 * given an input string, wrap the string in double quotes if there is
 * a space.  escape any existing double quotes.
 *
 * the behavior of this routine is subject to change but should remain
 * a reversible operation when used in conjunction with
 * bu_vls_decode().
 */
BU_EXPORT BU_EXTERN(void bu_vls_encode, (struct bu_vls *vp, const char *str));


/**
 * b u _ v l s _ d e c o d e
 *
 * given an encoded input string, unwrap the string from any
 * surrounding double quotes and unescape any embedded double quotes.
 *
 * the behavior of this routine is subject to change but should remain
 * the reverse operation of bu_vls_encode().
 */
BU_EXPORT BU_EXTERN(void bu_vls_decode, (struct bu_vls *vp, const char *str));


/** @} */
/** @addtogroup vlb */
/** @{ */

/**
 * Initialize the specified bu_vlb structure and mallocs the initial
 * block of memory.
 *
 * @param vlb Pointer to an unitialized bu_vlb structure
 */
BU_EXPORT BU_EXTERN(void bu_vlb_init,
		    (struct bu_vlb *vlb));

/**
 * Initialize the specified bu_vlb structure and mallocs the initial
 * block of memory with the specified size
 *
 * @param vlb Pointer to an unitialized bu_vlb structure
 * @param initialSize The desired initial size of the buffer
 */
BU_EXPORT BU_EXTERN(void bu_vlb_initialize,
		    (struct bu_vlb *vlb,
		     int initialSize));

/**
 * Write some bytes to the end of the bu_vlb structure. If necessary,
 * additional memory will be allocated.
 *
 * @param vlb Pointer to the bu_vlb structure to receive the bytes
 * @param start Pointer to the first byte to be copied to the bu_vlb structure
 * @param len The number of bytes to copy to the bu_vlb structure
 */
BU_EXPORT BU_EXTERN(void bu_vlb_write,
		    (struct bu_vlb *vlb,
		     unsigned char *start,
		     int len));

/**
 * Reset the bu_vlb counter to the start of its byte array. This
 * essentially ignores any bytes currenty in the buffer, but does not
 * free any memory.
 *
 * @param vlb Pointer to the bu_vlb structure to be reset
 */
BU_EXPORT BU_EXTERN(void bu_vlb_reset,
		    (struct bu_vlb *vlb));

/**
 * Get a pointer to the byte array held by the bu_vlb structure
 *
 * @param vlb Pointer to the bu_vlb structure
 * @return A pointer to the byte array contained by the bu_vlb structure
 */
BU_EXPORT BU_EXTERN(unsigned char *bu_vlb_getBuffer,
		    (struct bu_vlb *vlb));

/**
 * Return the number of bytes used in the bu_vlb structure
 *
 * @param vlb Pointer to the bu_vlb structure
 * @return The number of bytes written to the bu_vlb structure
 */
BU_EXPORT BU_EXTERN(int bu_vlb_getBufferLength,
		    (struct bu_vlb *vlb));

/**
 * Free the memory allocated for the byte array in the bu_vlb
 * structure.  Also unitializes the structure.
 *
 * @param vlb Pointer to the bu_vlb structure
 */
BU_EXPORT BU_EXTERN(void bu_vlb_free,
		    (struct bu_vlb *vlb));
/**
 * Write the current byte array from the bu_vlb structure to a file
 *
 * @param vlb Pointer to the bu_vlb structure that is the source of the bytes
 * @param fd Pointer to a FILE to receive the bytes
 */
BU_EXPORT BU_EXTERN(void bu_vlb_print,
		    (struct bu_vlb *vlb,
		     FILE *fd));

/** @file str.c
 *
 * Compatibility routines to various string processing functions
 * including strlcat and strlcpy.
 *
 */

/**
 * b u _ s t r l c a t / b u _ s t r l c a t m
 *
 * concatenate one string onto the end of another, returning the
 * length of the dst string after the concatenation.
 *
 * bu_strlcat() is a macro to bu_strlcatm() so that we can report the
 * file name and line number of any erroneous callers.
 */
BU_EXPORT BU_EXTERN(size_t bu_strlcatm, (char *dst, const char *src, size_t size, const char *label));
#define bu_strlcat(dst, src, size) bu_strlcatm(dst, src, size, BU_FLSTR)

/**
 * b u _ s t r l c p y / b u _ s t r l c p y m
 *
 * copies one string into another, returning the length of the dst
 * string after the copy.
 *
 * bu_strlcpy() is a macro to bu_strlcpym() so that we can report the
 * file name and line number of any erroneous callers.
 */
BU_EXPORT BU_EXTERN(size_t bu_strlcpym, (char *dst, const char *src, size_t size, const char *label));
#define bu_strlcpy(dst, src, size) bu_strlcpym(dst, src, size, BU_FLSTR)

/**
 * b u _ s t r d u p  / b u _ s t r d u p m
 *
 * Given a string, allocate enough memory to hold it using bu_malloc(),
 * duplicate the strings, returns a pointer to the new string.
 *
 * bu_strdup() is a macro that includes the current file name and line
 * number that can be used when bu debugging is enabled.
 */
BU_EXPORT BU_EXTERN(char *bu_strdupm, (const char *cp, const char *label));
#define bu_strdup(s) bu_strdupm(s, BU_FLSTR)

/** @} */

/** @addtogroup bu_log */
/** @{ */

/** @file units.c
 *
 * Module of libbu to handle units conversion between strings and mm.
 *
 */

/**
 * B U _ U N I T S _ C O N V E R S I O N
 *
 * Given a string representation of a unit of distance (eg, "feet"),
 * return the multiplier which will convert that unit into the default
 * unit for the dimension (millimeters for length, mm^3 for volume,
 * and grams for mass.)
 *
 * Returns 0.0 on error and >0.0 on success
 */
BU_EXPORT BU_EXTERN(double bu_units_conversion,
		    (const char *str));

/**
 * B U _ U N I T S _ S T R I N G
 *
 * Given a conversion factor to mm, search the table to find what unit
 * this represents.
 *
 * To accomodate floating point fuzz, a "near miss" is allowed.  The
 * algorithm depends on the table being sorted small-to-large.
 *
 * Returns -
 * char* units string
 * NULL	No known unit matches this conversion factor.
 */
BU_EXPORT BU_EXTERN(const char *bu_units_string,
		    (const double mm));
BU_EXPORT BU_EXTERN(struct bu_vls *bu_units_strings_vls,
		    ());

/**
 * B U _ N E A R E S T _ U N I T S _ S T R I N G
 *
 * Given a conversion factor to mm, search the table to find the
 * closest matching unit.
 *
 * Returns -
 * char* units string
 * NULL	Invalid conversion factor (non-positive)
 */
BU_EXPORT BU_EXTERN(const char *bu_nearest_units_string,
		    (const double mm));

/**
 * B U _ M M _ V A L U E
 *
 * Given a string of the form "25cm" or "5.2ft" returns the
 * corresponding distance in mm.
 *
 * Returns -
 * -1 on error
 * >0 on success
 */
BU_EXPORT BU_EXTERN(double bu_mm_value,
		    (const char *s));

/**
 * B U _ M M _ C V T
 *
 * Used primarily as a hooked function for bu_structparse tables to
 * allow input of floating point values in other units.
 */
BU_EXPORT BU_EXTERN(void bu_mm_cvt,
		    (const struct bu_structparse *sdp,
		     const char *name,
		     char *base,
		     const char *value));

/** @} */

/** @addtogroup hton */
/** @{ */
/** @file htester.c
 *
 * @brief
 * Test network float conversion.
 *
 * Expected to be used in pipes, or with TTCP links to other machines,
 * or with files RCP'ed between machines.
 *
 */

/** @file xdr.c
 *
 * Routines to implement an external data representation (XDR)
 * compatible with the usual InterNet standards, e.g.:
 * big-endian, twos-compliment fixed point, and IEEE floating point.
 *
 * Routines to insert/extract short/long's into char arrays,
 * independend of machine byte order and word-alignment.
 * Uses encoding compatible with routines found in libpkg,
 * and BSD system routines htonl(), htons(), ntohl(), ntohs().
 *
 */

/**
 * Macro version of library routine bu_glong()
 *
 * The argument is expected to be of type "unsigned char"
 */
#define BU_GLONGLONG(_cp)	\
	    ((((uint64_t)((_cp)[0])) << 56) |	\
	     (((uint64_t)((_cp)[1])) << 48) |	\
	     (((uint64_t)((_cp)[2])) << 40) |	\
	     (((uint64_t)((_cp)[3])) << 32) |	\
	     (((uint64_t)((_cp)[4])) << 24) |	\
	     (((uint64_t)((_cp)[5])) << 16) |	\
	     (((uint64_t)((_cp)[6])) <<  8) |	\
	      ((uint64_t)((_cp)[7])))
#define BU_GLONG(_cp)	\
	    ((((uint32_t)((_cp)[0])) << 24) |	\
	     (((uint32_t)((_cp)[1])) << 16) |	\
	     (((uint32_t)((_cp)[2])) <<  8) |	\
	      ((uint32_t)((_cp)[3])))
#define BU_GSHORT(_cp)	\
	    ((((uint16_t)((_cp)[0])) << 8) | \
		       (_cp)[1])

/**
 * B U _ G S H O R T
 */
BU_EXPORT BU_EXTERN(uint16_t bu_gshort, (const unsigned char *msgp));

/**
 * B U _ G L O N G
 */
BU_EXPORT BU_EXTERN(uint32_t bu_glong, (const unsigned char *msgp));

/**
 * B U _ P S H O R T
 */
BU_EXPORT BU_EXTERN(unsigned char *bu_pshort, (unsigned char *msgp, uint16_t s));

/**
 * B U _ P L O N G
 */
BU_EXPORT BU_EXTERN(unsigned char *bu_plong, (unsigned char *msgp, uint32_t l));

/**
 * B U _ P L O N G L O N G
 */
BU_EXPORT BU_EXTERN(unsigned char *bu_plonglong, (unsigned char *msgp, uint64_t l));

/** @} */

/** @addtogroup tcl */
/** @{ */
/** @file observer.c
 *
 * @brief
 * Routines for implementing the observer pattern.
 *
 */

/**
 * b u _ o b s e r v e r _ c m d
 *
 * runs a given command, calling the corresponding observer callback
 * if it matches.
 */
BU_EXPORT BU_EXTERN(int bu_observer_cmd, (ClientData clientData, Tcl_Interp *interp, int argc, const char *argv[]));

/**
 * b u _ o b s e r v e r _ n o t i f y
 *
 * Notify observers.
 */
BU_EXPORT BU_EXTERN(void bu_observer_notify, (Tcl_Interp *interp, struct bu_observer *headp, char *self));

/**
 * b u _ o b s e r v e r _ f r e e
 *
 * Free observers.
 */
BU_EXPORT BU_EXTERN(void bu_observer_free, (struct bu_observer *));

/** @file tcl.c
 *
 * Tcl interfaces to all the LIBBU Basic BRL-CAD Utility routines.
 *
 * Remember that in MGED you need to say "set glob_compat_mode 0" to
 * get [] to work with TCL semantics rather than MGED glob semantics.
 *
 */

/**
 * b u _ t c l _ s t r u c t p a r s e _ g e t _ t e r s e _ f o r m
 *
 * Convert the "form" of a bu_structparse table into a TCL result
 * string, with parameter-name data-type pairs:
 *
 * V {%f %f %f} A {%f %f %f}
 *
 * A different routine should build a more general 'form', along the
 * lines of:
 *
 * {V {%f %f %f} default {help}} {A {%f %f %f} default# {help}}
 *
 * @param interp tcl interpreter
 * @param sp structparse table
 *
 * 	@return
 * 	void
 */
BU_EXPORT BU_EXTERN(void bu_tcl_structparse_get_terse_form,
		    (Tcl_Interp *interp,
		     const struct bu_structparse *sp));

 /**
 * b u _ t c l _ s t r u c t p a r s e _ a r g v
 *
 * Support routine for db adjust and db put.  Much like the bu_struct_parse routine
 * which takes its input as a bu_vls. This routine, however, takes the arguments
 * as lists, a more Tcl-friendly method. Also knows about the Tcl result string,
 * so it can make more informative error messages.
 *
 * Operates on argv[0] and argv[1], then on argv[2] and argv[3], ...
 *
 *
 * @param interp	- tcl interpreter
 * @param argc	- number of elements in argv
 * @param argv	- contains the keyword-value pairs
 * @param desc	- structure description
 * @param base	- base addr of users struct
 *
 * 	@retval TCL_OK if successful,
 * @retval TCL_ERROR on failure
 */
BU_EXPORT BU_EXTERN(int bu_tcl_structparse_argv,
		    (Tcl_Interp *interp,
		     int argc,
		     char **argv,
		     const struct bu_structparse *desc,
		     char *base));

/**
 * b u _ t c l _ m e m _ b a r r i e r c h e c k
 *
 * A tcl wrapper for bu_mem_barriercheck.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_mem_barriercheck,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ c k _ m a l l o c _ p t r
 *
 * A tcl wrapper for bu_ck_malloc_ptr.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_ck_malloc_ptr,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ m a l l o c _ l e n _ r o u n d u p
 *
 * A tcl wrapper for bu_malloc_len_roundup.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_malloc_len_roundup,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ p r m e m
 *
 * A tcl wrapper for bu_prmem. Prints map of
 * memory currently in use, to stderr.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_prmem,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ p r i n t b
 *
 * A tcl wrapper for bu_vls_printb.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_printb,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ g e t _ v a l u e _ b y _ k e y w o r d
 *
 * Given arguments of alternating keywords and values and a specific
 * keyword ("Iwant"), return the value associated with that keyword.
 *
 * example: bu_get_value_by_keyword Iwant az 35 elev 25 temp 9.6
 *
 * If only one argument is given after the search keyword, it is
 * interpreted as a list in the same format.
 *
 * example: bu_get_value_by_keyword Iwant {az 35 elev 25 temp 9.6}
 *
 * Search order is left-to-right, only first match is returned.
 *
 * Sample use:
 * bu_get_value_by_keyword V8 [concat type [.inmem get box.s]]
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_get_value_by_keyword,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ g e t _ a l l _ k e y w o r d _ v a l u e s
 *
 * Given arguments of alternating keywords and values, establish local
 * variables named after the keywords, with the indicated
 * values. Returns in interp a list of the variable names that were
 * assigned to. This lets you detect at runtime what assignments were
 * actually performed.
 *
 * example: bu_get_all_keyword_values az 35 elev 25 temp 9.6
 *
 * This is much faster than writing this in raw Tcl 8 as:
 *
 * foreach {keyword value} $list {
 * 	set $keyword $value
 * 	lappend retval $keyword
 * }
 *
 * If only one argument is given it is interpreted as a list in the
 * same format.
 *
 * example:  bu_get_all_keyword_values {az 35 elev 25 temp 9.6}
 *
 * For security reasons, the name of the local variable assigned to is
 * that of the input keyword with "key_" prepended.  This prevents a
 * playful user from overriding variables inside the function,
 * e.g. loop iterator "i", etc.  This could be even worse when called
 * in global context.
 *
 * Processing order is left-to-right, rightmost value for a repeated
 * keyword will be the one used.
 *
 * Sample use:
 * bu_get_all_keyword_values [concat type [.inmem get box.s]]
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_get_all_keyword_values,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ r g b _ t o _ h s v
 *
 * A tcl wrapper for bu_rgb_to_hsv.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_rgb_to_hsv,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ h s v _ t o _ r g b
 *
 * A tcl wrapper for bu_hsv_to_rgb.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_hsv_to_rgb,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ k e y _ e q _ t o _ k e y _ v a l
 *
 * Converts key=val to "key val" pairs.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_key_eq_to_key_val,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ s h a d e r _ t o _ k e y _ v a l
 *
 * Converts a shader string to a tcl list.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_shader_to_key_val,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ k e y _ v a l _ t o _ k e y _ e q
 *
 * Converts "key value" pairs to key=value.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_key_val_to_key_eq,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * b u _ t c l _ s h a d e r _ t o _ k e y _ e q
 *
 * Converts a shader tcl list into a shader string.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_shader_to_key_eq,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * bu_tcl_brlcad_root
 *
 * A tcl wrapper for bu_brlcad_root.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_brlcad_root,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * bu_tcl_brlcad_data
 *
 * A tcl wrapper for bu_brlcad_data.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_brlcad_data,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * bu_tcl_units_conversion
 *
 * A tcl wrapper for bu_units_conversion.
 *
 * @param clientData	- associated data/state
 * @param interp	- tcl interpreter in which this command was registered.
 * @param argc		- number of elements in argv
 * @param argv		- command name and arguments
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int bu_tcl_units_conversion,
		    (ClientData clientData,
		     Tcl_Interp *interp,
		     int argc,
		     const char **argv));

/**
 * bu_tcl_setup
 *
 * Add all the supported Tcl interfaces to LIBBU routines to the list
 * of commands known by the given interpreter.
 *
 * @param interp	- tcl interpreter in which this command was registered.
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(void bu_tcl_setup,
		    (Tcl_Interp *interp));

/**
 * Bu_Init
 *
 * Allows LIBBU to be dynamically loaded to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbu.so"
 *
 * @param interp	- tcl interpreter in which this command was registered.
 *
 * @return TCL_OK if successful, otherwise, TCL_ERROR.
 */
BU_EXPORT BU_EXTERN(int Bu_Init,
		    (Tcl_Interp *interp));


/** @} */

/** @addtogroup bu_log */
/** @{ */
/** @file lex.c
 *
 */

#define BU_LEX_ANY 0	/* pseudo type */
struct bu_lex_t_int {
    int type;
    int value;
};
#define BU_LEX_INT 1
struct bu_lex_t_dbl {
    int type;
    double value;
};
#define BU_LEX_DOUBLE 2
struct bu_lex_t_key {
    int type;
    int value;
};
#define BU_LEX_SYMBOL 3
#define BU_LEX_KEYWORD 4
struct bu_lex_t_id {
    int type;
    char *value;
};
#define BU_LEX_IDENT 5
#define BU_LEX_NUMBER 6	/* Pseudo type */
union bu_lex_token {
    int type;
    struct bu_lex_t_int t_int;
    struct bu_lex_t_dbl t_dbl;
    struct bu_lex_t_key t_key;
    struct bu_lex_t_id t_id;
};
struct bu_lex_key {
    int tok_val;
    char *string;
};
#define BU_LEX_NEED_MORE 0


/**
 * B U _ L E X
 */
BU_EXPORT BU_EXTERN(int bu_lex,
		    (union bu_lex_token *token,
		     struct bu_vls *rtstr,
		     struct bu_lex_key *keywords,
		     struct bu_lex_key *symbols));


/** @file mread.c
 *
 * @brief multiple-read to fill a buffer
 *
 * Provide a general means to a read some count of items from a file
 * descriptor reading multiple times until the quantity desired is
 * obtained.  This is useful for pipes and network connections that
 * don't necessarily deliver data with the same grouping as it is
 * written with.
 *
 * If a read error occurs, a negative value will be returns and errno
 * should be set (by read()).
 *
 */

/**
 * "Multiple try" read.  Read multiple times until quantity is
 * obtained or an error occurs.  This is useful for pipes.
 */
BU_EXPORT BU_EXTERN(long int bu_mread,
		    (int fd, void *bufp, long int n));

/** @} */

/** @addtogroup mro */
/** @{ */
/** @file mro.c
 *
 * @brief
 * The Multiply Represented Object package.
 *
 */

BU_EXPORT BU_EXTERN(void bu_mro_init_with_string,
		    (struct bu_mro *mrop, const char *string));
BU_EXPORT BU_EXTERN(void bu_mro_set,
		    (struct bu_mro *mrop,
		     const char *string));
BU_EXPORT BU_EXTERN(void bu_mro_init,
		    (struct bu_mro *mrop));
BU_EXPORT BU_EXTERN(void bu_mro_free,
		    (struct bu_mro *mrop));


/** @} */

/** @addtogroup bu_hash */
/** @{ */
/** @file hash.c
 *
 * @brief
 * An implimentation of hash tables.
 */

struct bu_hash_entry {
    unsigned long magic;
    unsigned char *key;
    unsigned char *value;
    int key_len;
    struct bu_hash_entry *next;
};

struct bu_hash_tbl {
    unsigned long magic;
    unsigned long mask;
    unsigned long num_lists;
    unsigned long num_entries;
    struct bu_hash_entry **lists;
};

struct bu_hash_record {
    unsigned long magic;
    struct bu_hash_tbl *tbl;
    unsigned long index;
    struct bu_hash_entry *hsh_entry;
};

#define BU_CK_HASH_TBL(_hp)	BU_CKMAG(_hp, BU_HASH_TBL_MAGIC, "bu_hash_tbl")
#define BU_CK_HASH_RECORD(_rp)	BU_CKMAG(_rp, BU_HASH_RECORD_MAGIC, "bu_hash_record")
#define BU_CK_HASH_ENTRY(_ep)	BU_CKMAG(_ep, BU_HASH_ENTRY_MAGIC, "bu_hash_entry")

/**
 * B U _ H A S H
 *
 * the hashing function
 */
BU_EXPORT BU_EXTERN(unsigned long bu_hash,
		    (unsigned char *str,
		     int len));

/**
 * B U _ C R E A T E _ H A S H _ T B L
 * @brief
 * Create an empty hash table
 *
 * The input is the number of desired hash bins.  This number will be
 * rounded up to the nearest power of two.
 */
BU_EXPORT BU_EXTERN(struct bu_hash_tbl *bu_create_hash_tbl,
		    (unsigned long tbl_size));

/**
 * B U _ F I N D _ H A S H _ E N T R Y
 *
 * @brief
 * Find the hash table entry corresponding to the provided key
 *
 * @param[in] hsh_tbl - The hash table to look in
 * @param[in] key - the key to look for
 * @param[in] key_len - the length of the key in bytes
 *
 * Output:
 * @param[out] prev - the previous hash table entry (non-null for entries that not the first in hash bin)
 * @param[out] idx - the index of the hash bin for this key
 *
 * @return
 * the hash table entry corresponding to the provided key, or NULL if
 * not found.
 */
BU_EXPORT BU_EXTERN(struct bu_hash_entry *bu_find_hash_entry,
		    (struct bu_hash_tbl *hsh_tbl,
		     unsigned char *key,
		     int key_len,
		     struct bu_hash_entry **prev,
		     unsigned long *index2));

/**
 * B U _ S E T _ H A S H _ V A L U E
 * @brief
 * Set the value for a hash table entry
 *
 * Note that this is just a pointer copy, the hash table does not
 * maintain its own copy of this value.
 */
BU_EXPORT BU_EXTERN(void bu_set_hash_value,
		    (struct bu_hash_entry *hsh_entry,
		     unsigned char *value));

/**
 * B U _ G E T _ H A S H _ V A L U E
 *
 * get the value pointer stored for the specified hash table entry
 */
BU_EXPORT BU_EXTERN(unsigned char *bu_get_hash_value,
		    (struct bu_hash_entry *hsh_entry));

/**
 * B U _ G E T _ H A S H _ K E Y
 *
 * get the key pointer stored for the specified hash table entry
 */
BU_EXPORT BU_EXTERN(unsigned char *bu_get_hash_key,
		    (struct bu_hash_entry *hsh_entry));

/**
 * B U _ H A S H _ A D D _ E N T R Y
 *
 * Add an new entry to a hash table
 *
 * @param[in] hsh_tbl - the hash table to accept thye new entry
 * @param[in] key - the key (any byte string)
 * @param[in] key_len - the number of bytes in the key
 *
 * @param[out] new - a flag, non-zero indicates that a new entry was
 * created.  zero indicates that an entry already exists with the
 * specified key and key length
 *
 * @return
 * a hash table entry. If "new" is non-zero, a new, empty entry is
 * returned.  if "new" is zero, the returned entry is the one matching
 * the specified key and key_len.
 */
BU_EXPORT BU_EXTERN(struct bu_hash_entry *bu_hash_add_entry,
		    (struct bu_hash_tbl *hsh_tbl,
		     unsigned char *key,
		     int key_len,
		     int *new_entry));

/**
 * B U _ H A S H _ T B L _ P R
 * @brief
 * Print the specified hash table to stderr.
 *
 * (Note that the keys and values are printed as pointers)
 */
BU_EXPORT BU_EXTERN(void bu_hash_tbl_pr,
		    (struct bu_hash_tbl *hsh_tbl,
		     char *str));

/**
 * B U _ H A S H _ T B L _ F R E E
 * @brief
 * Free all the memory associated with the specified hash table.
 *
 * Note that the keys are freed (they are copies), but the "values"
 * are not freed.  (The values are merely pointers)
 */
BU_EXPORT BU_EXTERN(void bu_hash_tbl_free,
		    (struct bu_hash_tbl *hsh_tbl));

/**
 * B U _ H A S H _ T B L _ F I R S T
 * @brief
 * get the "first" entry in a hash table
 *
 * @param[in] hsh_tbl - the hash table of interest
 * @param[in] rec - an empty "bu_hash_record" structure for use by this routine and "bu_hash_tbl_next"
 *
 * @return
 * the first non-null entry in the hash table, or NULL if there are no
 * entries (Note that the order of enties is not likely to have any
 * significance)
 */
BU_EXPORT BU_EXTERN(struct bu_hash_entry *bu_hash_tbl_first,
		    (struct bu_hash_tbl *hsh_tbl,
		     struct bu_hash_record *rec));

/**
 * B U _ H A S H _ T B L _ N E X T
 *
 * get the "next" entry in a hash table
 *
 * input:
 * rec - the "bu_hash_record" structure that was passed to
 * "bu_hash_tbl_first"
 *
 * return:
 * the "next" non-null hash entry in this hash table
 */
BU_EXPORT BU_EXTERN(struct bu_hash_entry *bu_hash_tbl_next,
		    (struct bu_hash_record *rec));


/** @} */

/** @addtogroup image */
/** @{ */
/** @file image.c
 *
 * @brief image save/load routines
 *
 * save or load images in a variety of formats.
 *
 */

enum {
    BU_IMAGE_AUTO,
    BU_IMAGE_PIX,
    BU_IMAGE_BW,
    BU_IMAGE_ALIAS,
    BU_IMAGE_BMP,
    BU_IMAGE_CI,
    BU_IMAGE_ORLE,
    BU_IMAGE_PNG,
    BU_IMAGE_PPM,
    BU_IMAGE_PS,
    BU_IMAGE_RLE,
    BU_IMAGE_SPM,
    BU_IMAGE_SUN,
    BU_IMAGE_YUV
};


struct bu_image_file {
    unsigned long magic;
    char *filename;
    int fd;
    int format;			/* BU_IMAGE_* */
    int width, height, depth;	/* pixel, pixel, byte */
    unsigned char *data;
    unsigned long flags;
};

BU_EXPORT BU_EXTERN(struct bu_image_file *bu_image_save_open,
		    (char *filename,
		     int format,
		     int width,
		     int height,
		     int depth));

BU_EXPORT BU_EXTERN(int bu_image_save_writeline,
		    (struct bu_image_file *bif,
		     int y,
		     unsigned char *data));

BU_EXPORT BU_EXTERN(int bu_image_save_close,
		    (struct bu_image_file *bif));

BU_EXPORT BU_EXTERN(int bu_image_save,
		    (unsigned char *data,
		     int width,
		     int height,
		     int depth,
		     char *filename,
		     int filetype));

/** @} */
/* end image utilities */

/** @addtogroup file */
/** @{ */
/** @file fchmod.c
 *
 * Wrapper around fchmod.
 *
 */

/**
 * b u _ f c h m o d
 *
 */
BU_EXPORT BU_EXTERN(int bu_fchmod,
		    (FILE *fp, unsigned long pmode));

/** @file argv.c
 *
 * Functions related to argv processing.
 *
 */

/**
 * Deallocate all strings in a given argv array and the array itself
 *
 * This call presumes the array has been allocated with bu_dup_argv()
 * or bu_argv_from_path().
 */
BU_EXPORT BU_EXTERN(void bu_free_argv, (int argc, char *argv[]));

/**
 * b u _ f r e e _ a r r a y
 *
 * free up to argc elements of memory allocated to an array without
 * free'ing the array itself.
 */
BU_EXPORT BU_EXTERN(void bu_free_array, (int argc, char *argv[], const char *str));

/**
 * Dynamically duplicate an argv array and all elements in the array
 *
 * Duplicate an argv array by duplicating all strings and the array
 * itself.  It is the caller's responsibility to free the array
 * returned including all elements in the array by calling bu_free()
 * or bu_free_argv().
 */
BU_EXPORT BU_EXTERN(char **bu_dup_argv, (int argc, const char *argv[]));

/**
 * Combine two argv arrays into one new (duplicated) argv array.
 *
 * If insert is negative, the insertArgv array elements will be
 * prepended into the new argv array.  If insert is greater than or
 * equal to argc, the insertArgv array elements will be appended after
 * all duplicated elementes in the specified argv array.  Otherwise,
 * the insert argument is the position where the insertArgv array
 * elements will be merged with the specified argv array.
 */
BU_EXPORT BU_EXTERN(char **bu_dupinsert_argv, (int insert, int insertArgc, const char *insertArgv[], int argc, const char *argv[]));

/**
 * Generate an argv array from a path
 *
 * Given a path string, separate the path elements into a dynamically
 * allocated argv array with the path separators removed.  It is the
 * caller's responsibility to free the array that is returned as well
 * as all elements in the array using bu_free_argv() or manually
 * calling bu_free().
 */
BU_EXPORT BU_EXTERN(char **bu_argv_from_path, (const char *path, int *ac));


/** @file interrupt.c
 *
 * Routines for managing signals.  In particular, provide a common
 * means to temporarily buffer processing a signal during critical
 * write operations.
 *
 */

/**
 * Defer signal processing and interrupts before critical sections.
 *
 * Signal processing for a variety of signals that would otherwise
 * disrupt the logic of an application are put on hold until
 * bu_restore_interrupts() is called.
 *
 * If an interrupt signal is received while suspended, it will be
 * raised when/if interrupts are restored.
 *
 * Returns 0 on success.
 * Returns non-zero on error (with perror set if signal() failure).
 */
BU_EXPORT BU_EXTERN(int bu_suspend_interrupts, ());

/**
 * Resume signal processing and interrupts after critical sections.
 *
 * If a signal was raised since bu_suspend_interrupts() was called,
 * the previously installed signal handler will be immediately called
 * albeit only once even if multiple signals were received.
 *
 * Returns 0 on success.
 * Returns non-zero on error (with perror set if signal() failure).
 */
BU_EXPORT BU_EXTERN(int bu_restore_interrupts, ());

/** @} */

/** @addtogroup file */
/** @{ */
/** @file simd.c
 * Detect SIMD type at runtime.
 */

#define BU_SIMD_ALTIVEC 4
#define BU_SIMD_SSE2 3
#define BU_SIMD_SSE 2
#define BU_SIMD_MMX 1
#define BU_SIMD_NONE 0
BU_EXPORT BU_EXTERN(int bu_simd_level, ());

/** @} */


__END_DECLS

#endif  /* __BU_H__ */

/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
