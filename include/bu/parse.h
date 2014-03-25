/*                      P A R S E . H
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

/** @file parse.h
 *
 */
#ifndef BU_PARSE_H
#define BU_PARSE_H

#include "common.h"
#include <stdio.h> /* For FILE */
#include <stddef.h> /* for size_t */

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/vls.h"

__BEGIN_DECLS

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
#  define offsetof(_t, _m) (size_t)(&(((_t *)0)->_m))
#endif
#define bu_offsetof(_t, _m) (size_t)offsetof(_t, _m)
#define bu_offsetofarray(_t, _a, _d, _i) bu_offsetof(_t, _a) + sizeof(_d) * _i


/**
 * Convert address of global data object into byte "offset" from
 * address 0.
 *
 * Strictly speaking, the C language only permits initializers of the
 * form: address +- constant, where here the intent is to measure the
 * byte address of the indicated variable.  Matching compensation code
 * for the CRAY is located in librt/parse.c
 */
#if defined(__ia64__) || defined(__x86_64__) || defined(__sparc64__) || defined(_HPUX_SOURCE) || defined(__clang__)
#    define bu_byteoffset(_i)	((size_t)((char *)&(_i)))
#else
/* "Conservative" way of finding # bytes as diff of 2 char ptrs */
#  define bu_byteoffset(_i)	((size_t)(((char *)&(_i))-((char *)0)))
#endif


/**
 * The "bu_structparse" struct describes one element of a structure.
 * Collections of these are combined to describe entire structures (or at
 * least those portions for which parse/print/import/export support is
 * desired.
 *
 * Provides a convenient way of describing a C data structure, and
 * reading and writing it in both human-readable ASCII and efficient
 * binary forms.
 *
 * For example:
 *
 @code

 struct data_structure {
   char a_char;
   char str[32];
   short a_short;
   int a_int;
   fastf_t a_fastf_t;
   double a_double;
 }

 struct data_structure default = { 'c', "the default string", 32767, 1, 1.0, 1.0 };

 struct data_structure my_values;

 struct bu_structparse data_sp[] ={
   {"%c", 1,     "a_char",   bu_offsetof(data_structure, a_char), BU_STRUCTPARSE_FUNC_NULL,                      "a single character", (void*)&default.a_char},
   {"%s", 32,       "str", bu_offsetofarray(data_structure, str), BU_STRUCTPARSE_FUNC_NULL,         "This is a full character string", (void*)default.str},
   {"%i", 1,    "a_short",  bu_offsetof(data_structure, a_short), BU_STRUCTPARSE_FUNC_NULL,                         "A 16bit integer", (void*)&default.a_short},
   {"%d", 1,      "a_int",    bu_offsetof(data_structure, a_int), BU_STRUCTPARSE_FUNC_NULL,                          "A full integer", (void*)&default.a_int},
   {"%f", 1,   "a_fastf_t", bu_offsetof(data_structure, a_fastf_t), BU_STRUCTPARSE_FUNC_NULL, "A variable-precision fasf_t floating point value", (void*)&default.a_fastf_t},
   {"%g", 1,   "a_double", bu_offsetof(data_structure, a_double), BU_STRUCTPARSE_FUNC_NULL, "A double-precision fasf_t floating point value", (void*)&default.a_double},
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
    const char sp_fmt[4];		/**< "%i" or "%f", etc. */
    size_t sp_count;		/**< number of elements */
    const char *sp_name;		/**< Element's symbolic name */
    size_t sp_offset;		/**< Byte offset in struct */
    void (*sp_hook)(const struct bu_structparse *,
		    const char *,
		    void *,
		    const char *);	/**< Optional hooked function, or indir ptr */
    const char *sp_desc;		/**< description of element */
    void *sp_default;		       /**< ptr to default value */
};
typedef struct bu_structparse bu_structparse_t;
#define BU_STRUCTPARSE_NULL ((struct bu_structparse *)0)

#define BU_STRUCTPARSE_FUNC_NULL ((void(*)(const struct bu_structparse *, const char *, void *, const char *))0)

/**
 * assert the integrity of a bu_structparse struct.
 */
#define BU_CK_STRUCTPARSE(_sp) /* nothing to do */

/**
 * initialize a bu_structparse struct without allocating any memory.
 */
#define BU_STRUCTPARSE_INIT(_sp) { \
	(_sp)->sp_fmt[0] = (_sp)->sp_fmt[1] = (_sp)->sp_fmt[2] = (_sp)->sp_fmt[3] = '\0'; \
	(_sp)->sp_count = 0; \
	(_sp)->sp_name = NULL; \
	(_sp)->sp_offset = 0; \
	(_sp)->sp_hook = BU_STRUCTPARSE_FUNC_NULL; \
	(_sp)->sp_desc = NULL; \
	(_sp)->sp_default = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a bu_structparse
 * struct.  does not allocate memory.
 */
#define BU_STRUCTPARSE_INIT_ZERO { {'\0', '\0', '\0', '\0'}, 0, NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }

/**
 * returns truthfully whether a bu_structparse struct has been
 * initialized.  validates whether pointer is non-NULL.
 */
#define BU_STRUCTPARSE_IS_INITIALIZED(_sp) ((struct bu_structparse *)(_sp) != BU_STRUCTPARSE_NULL)


/*----------------------------------------------------------------------*/
/**
 * An "opaque" handle for holding onto objects, typically in some kind
 * of external form that is not directly usable without passing
 * through an "importation" function.
 *
 * A "bu_external" struct holds the "external binary" representation
 * of a structure or other block of arbitrary data.
 */
struct bu_external  {
    uint32_t ext_magic;
    size_t ext_nbytes;
#if defined(USE_BINARY_ATTRIBUTES)
    unsigned char widcode; /* needed for decoding binary attributes,
			    * same type as 'struct
			    * db5_raw_internal.a_width' */
#endif
    uint8_t *ext_buf;
};
typedef struct bu_external bu_external_t;
#define BU_EXTERNAL_NULL ((struct bu_external *)0)

/**
 * assert the integrity of a bu_external struct.
 */
#define BU_CK_EXTERNAL(_p) BU_CKMAG(_p, BU_EXTERNAL_MAGIC, "bu_external")

/**
 * initializes a bu_external struct without allocating any memory.
 */
#define BU_EXTERNAL_INIT(_p) { \
	(_p)->ext_magic = BU_EXTERNAL_MAGIC; \
	(_p)->ext_nbytes = 0; \
	(_p)->ext_buf = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_external struct. does not allocate memory.
 */
#define BU_EXTERNAL_INIT_ZERO { BU_EXTERNAL_MAGIC, 0, NULL }

/**
 * returns truthfully whether a bu_external struct has been
 * initialized.  is not reliable unless the struct has been
 * initialized with BU_EXTERNAL_INIT().
 */
#define BU_EXTERNAL_IS_INITIALIZED(_p) (((struct bu_external *)(_p) != BU_EXTERNAL_NULL) && (_p)->ext_magic == BU_EXTERNAL_MAGIC)

/** @file libbu/parse.c
 *
 * routines for parsing arbitrary structures
 *
 * Routines to assign values to elements of arbitrary structures.  The
 * layout of a structure to be processed is described by a structure
 * of type "bu_structparse", giving element names, element formats, an
 * offset from the beginning of the structure, and a pointer to an
 * optional "hooked" function that is called whenever that structure
 * element is changed.
 */

/**
 * ASCII to struct elements.
 *
 * Parse the structure element description in the vls string "vls"
 * according to the structure description in "parsetab"
 *
 * @return <0 failure
 * @return  0 OK
 */
BU_EXPORT extern int bu_struct_parse(const struct bu_vls *in_vls,
				     const struct bu_structparse *desc,
				     const char *base);

/**
 * struct elements to ASCII.
 */
BU_EXPORT extern void bu_struct_print(const char *title,
				      const struct bu_structparse *parsetab,
				      const char *base);

/**
 * struct elements to machine-independent binary.
 *
 * copies ext data to base
 */
BU_EXPORT extern int bu_struct_export(struct bu_external *ext,
				      const genptr_t base,
				      const struct bu_structparse *imp);

/**
 * machine-independent binary to struct elements.
 *
 * copies ext data to base
 */
BU_EXPORT extern int bu_struct_import(genptr_t base,
				      const struct bu_structparse *imp,
				      const struct bu_external *ext);

/**
 * Put a structure in external form to a stdio file.  All formatting
 * must have been accomplished previously.
 *
 * Returns number of bytes written.  On error, a short byte count (or
 * zero) is returned.  Use feof(3) or ferror(3) to determine which
 * errors occur.
 */
BU_EXPORT extern size_t bu_struct_put(FILE *fp,
				      const struct bu_external *ext);

/**
 * Obtain the next structure in external form from a stdio file.
 *
 * Returns number of bytes read into the bu_external.  On error, zero
 * is returned.
 */
BU_EXPORT extern size_t bu_struct_get(struct bu_external *ext,
				      FILE *fp);

/**
 * Given a buffer with an external representation of a structure
 * (e.g. the ext_buf portion of the output from bu_struct_export),
 * check it for damage in shipment, and if it's OK, wrap it up in an
 * bu_external structure, suitable for passing to bu_struct_import().
 */
BU_EXPORT extern void bu_struct_wrap_buf(struct bu_external *ext,
					 genptr_t buf);

/**
 * This differs from bu_struct_print in that this output is less
 * readable by humans, but easier to parse with the computer.
 */
BU_EXPORT extern void bu_vls_struct_print(struct bu_vls *vls,
					  const struct bu_structparse *sdp,
					  const char *base);

/**
 * This differs from bu_struct_print in that it prints to a vls.
 */
BU_EXPORT extern void bu_vls_struct_print2(struct bu_vls *vls,
					   const char *title,
					   const struct bu_structparse *sdp,
					   const char *base);

/**
 * Convert a structure element (indicated by sdp) to its ASCII
 * representation in a VLS.
 */
BU_EXPORT extern void bu_vls_struct_item(struct bu_vls *vp,
					 const struct bu_structparse *sdp,
					 const char *base,
					 int sep_char);

/**
 * Convert a structure element called "name" to an ASCII
 * representation in a VLS.
 */
BU_EXPORT extern int bu_vls_struct_item_named(struct bu_vls *vp,
					      const struct bu_structparse *sdp,
					      const char *name,
					      const char *base,
					      int sep_char);

/**
 * This allows us to specify the "size" parameter as values like ".5m"
 * or "27in" rather than using mm all the time.
 */
BU_EXPORT extern void bu_parse_mm(const struct bu_structparse *sdp,
				  const char *name,
				  char *base,
				  const char *value);

BU_EXPORT extern int bu_key_eq_to_key_val(const char *in,
					  const char **next,
					  struct bu_vls *vls);

/**
 * Take an old v4 shader specification of the form
 *
 *   shadername arg1=value1 arg2=value2 color=1/2/3
 *
 * and convert it into the v5 {} list form
 *
 *   shadername {arg1 value1 arg2 value2 color 1/2/3}
 *
 * Note -- the input string is smashed with nulls.
 *
 * Note -- the v5 version is used everywhere internally, and in v5
 * databases.
 *
 * @return 1 error
 * @return 0 OK
 */
BU_EXPORT extern int bu_shader_to_list(const char *in, struct bu_vls *vls);

BU_EXPORT extern int bu_shader_to_key_eq(const char *in, struct bu_vls *vls);

/**
 * Take a block of memory, and write it into a file.
 *
 * Caller is responsible for freeing memory of external representation,
 * using bu_free_external().
 *
 * @return <0 error
 * @return  0 OK
 */
BU_EXPORT extern int bu_fwrite_external(FILE *fp,
					const struct bu_external *ep);

BU_EXPORT extern void bu_hexdump_external(FILE *fp, const struct bu_external *ep,
					  const char *str);

BU_EXPORT extern void bu_free_external(struct bu_external *ep);

BU_EXPORT extern void bu_copy_external(struct bu_external *op,
				       const struct bu_external *ip);

/**
 * Advance pointer through string over current token,
 * across white space, to beginning of next token.
 */
BU_EXPORT extern char *bu_next_token(char *str);

BU_EXPORT extern void bu_structparse_get_terse_form(struct bu_vls *logstr,
						    const struct bu_structparse *sp);

/**
 * Support routine for db adjust and db put.  Much like the
 * bu_struct_parse routine which takes its input as a bu_vls. This
 * routine, however, takes the arguments as lists, a more Tcl-friendly
 * method. There is a log vls for storing messages.
 *
 * Operates on argv[0] and argv[1], then on argv[2] and argv[3], ...
 *
 * @param str  - vls for dumping info that might have gone to bu_log
 * @param argc - number of elements in argv
 * @param argv - contains the keyword-value pairs
 * @param desc - structure description
 * @param base - base addr of users struct
 *
 * @retval BRLCAD_OK if successful,
 * @retval BRLCAD_ERROR on failure
 */
BU_EXPORT extern int bu_structparse_argv(struct bu_vls *str,
					 int argc,
					 const char **argv,
					 const struct bu_structparse *desc,
					 char *base);

/**
 * Skip the separator(s) (i.e. whitespace and open-braces)
 *
 * @param _cp	- character pointer
 */
#define BU_SP_SKIP_SEP(_cp) { \
	while (*(_cp) && (*(_cp) == ' ' || *(_cp) == '\n' || \
			  *(_cp) == '\t' || *(_cp) == '{'))  ++(_cp); \
    }


/** @file libbu/booleanize.c
 *
 * routines for parsing boolean values from strings
 */

/**
 * Returns truthfully if a given input string represents an
 * "affirmative string".
 *
 * Input values that are null, empty, begin with the letter 'n', or
 * are 0-valued return as false.  Any other input value returns as
 * true.  Strings that strongly indicate true return as 1, other
 * values still return as true but may be a value greater than 1.
 */
BU_EXPORT extern int bu_str_true(const char *str);

/**
 * Returns truthfully if a given input string represents a
 * "negative string".
 *
 * Input values that are null, empty, begin with the letter 'n', or
 * are 0-valued return as true.  Any other input value returns as
 * false.
 */
BU_EXPORT extern int bu_str_false(const char *str);


/** @} */

__END_DECLS

#endif  /* BU_PARSE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
