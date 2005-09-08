/* acconfig.h
   This file is in the public domain.

   Descriptive text for the C preprocessor macros that
   the distributed Autoconf macros can define.
   No software package will use all of them; autoheader copies the ones
   your configure.in uses into your configuration header file templates.

   The entries are in sort -df order: alphabetical, case insensitive,
   ignoring punctuation (such as underscores).  Although this order
   can split up related entries, it makes it easier to check whether
   a given entry is in the file.

   Leave the following blank line there!!  Autoheader needs it.  */


#define BLT_CONFIG_H  	1		

/* Define if DBL_EPSILON is not defined in float.h */
#undef BLT_DBL_EPSILON

/* Define if drand48 isn't declared in math.h. */
#undef NEED_DECL_DRAND48

/* Define if srand48 isn't declared in math.h. */
#undef NEED_DECL_SRAND48

/* Define if strdup isn't declared in a standard header file. */
#undef NEED_DECL_STRDUP

/* Define if j1 isn't declared in a standard header file. */
#undef NEED_DECL_J1

/* Define if union wait type is defined incorrectly.  */
#undef HAVE_UNION_WAIT

/* Define if isfinite is found in libm.  */
#undef HAVE_ISFINITE

