/*                      D B _ I O . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file rt/db_io.h
 *
 */

#ifndef RT_DB_IO_H
#define RT_DB_IO_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "vmath.h"
#include "bu/avs.h"
#include "rt/db5.h"
#include "rt/defines.h"

__BEGIN_DECLS


struct rt_db_internal; /* forward declaration */


/* db_open.c */
/**
 * Ensure that the on-disk database has been completely written out of
 * the operating system's cache.
 */
RT_EXPORT extern void db_sync(struct db_i	*dbip);


/**
 * for db_open(), open the specified file as read-only
 */
#define DB_OPEN_READONLY "r"

/**
 * for db_open(), open the specified file as read-write
 */
#define DB_OPEN_READWRITE "rw"

/**
 * Open the named database.
 *
 * The 'name' parameter specifies the file or filepath to a .g
 * geometry database file for reading and/or writing.
 *
 * The 'mode' parameter specifies whether to open read-only or in
 * read-write mode, specified via the DB_OPEN_READONLY and
 * DB_OPEN_READWRITE symbols respectively.
 *
 * As a convenience, the returned db_t structure's dbi_filepath field
 * is a C-style argv array of dirs to search when attempting to open
 * related files (such as data files for EBM solids or texture-maps).
 * The default values are "." and the directory containing the ".g"
 * file.  They may be overridden by setting the environment variable
 * BRLCAD_FILE_PATH.
 *
 * Returns:
 * DBI_NULL error
 * db_i * success
 */
RT_EXPORT extern struct db_i *
db_open(const char *name, const char *mode);


/* create a new model database */
/**
 * Create a new database containing just a header record, regardless
 * of whether the database previously existed or not, and open it for
 * reading and writing.
 *
 * This routine also calls db_dirbuild(), so the caller doesn't need
 * to.
 *
 * Returns:
 * DBI_NULL on error
 * db_i * on success
 */
RT_EXPORT extern struct db_i *db_create(const char *name,
					int version);

/* close a model database */
/**
 * De-register a client of this database instance, if provided, and
 * close out the instance.
 */
RT_EXPORT extern void db_close_client(struct db_i *dbip,
				      long *client);

/**
 * Close a database, releasing dynamic memory Wait until last user is
 * done, though.
 */
RT_EXPORT extern void db_close(struct db_i *dbip);


/* dump a full copy of a database */
/**
 * Dump a full copy of one database into another.  This is a good way
 * of committing a ".inmem" database to a ".g" file.  The input is a
 * database instance, the output is a LIBWDB object, which could be a
 * disk file or another database instance.
 *
 * Returns -
 * -1 error
 * 0 success
 */
RT_EXPORT extern int db_dump(struct rt_wdb *wdbp,
			     struct db_i *dbip);

/**
 * Obtain an additional instance of this same database.  A new client
 * is registered at the same time if one is specified.
 */
RT_EXPORT extern struct db_i *db_clone_dbi(struct db_i *dbip,
					   long *client);


/**
 * Create a v5 database "free" object of the specified size, and place
 * it at the indicated location in the database.
 *
 * There are two interesting cases:
 * - The free object is "small".  Just write it all at once.
 * - The free object is "large".  Write header and trailer
 * separately
 *
 * @return 0 OK
 * @return -1 Fail.  This is a horrible error.
 */
RT_EXPORT extern int db5_write_free(struct db_i *dbip,
				    struct directory *dp,
				    size_t length);


/**
 * Change the size of a v5 database object.
 *
 * If the object is getting smaller, break it into two pieces, and
 * write out free objects for both.  The caller is expected to
 * re-write new data on the first one.
 *
 * If the object is getting larger, seek a suitable "hole" large
 * enough to hold it, throwing back any surplus, properly marked.
 *
 * If the object is getting larger and there is no suitable "hole" in
 * the database, extend the file, write a free object in the new
 * space, and write a free object in the old space.
 *
 * There is no point to trying to extend in place, that would require
 * two searches through the memory map, and doesn't save any disk I/O.
 *
 * Returns -
 * 0 OK
 * -1 Failure
 */
RT_EXPORT extern int db5_realloc(struct db_i *dbip,
				 struct directory *dp,
				 struct bu_external *ep);


/**
 * A routine for merging together the three optional parts of an
 * object into the final on-disk format.  Results in extra data
 * copies, but serves as a starting point for testing.  Any of name,
 * attrib, and body may be null.
 */
RT_EXPORT extern void db5_export_object3(struct bu_external *out,
					 int			dli,
					 const char			*name,
					 const unsigned char	hidden,
					 const struct bu_external	*attrib,
					 const struct bu_external	*body,
					 int			major,
					 int			minor,
					 int			a_zzz,
					 int			b_zzz);


/**
 * The attributes are taken from ip->idb_avs
 *
 * If present, convert attributes to on-disk format.  This must happen
 * after exporting the body, in case the ft_export5() method happened
 * to extend the attribute set.  Combinations are one "solid" which
 * does this.
 *
 * The internal representation is NOT freed, that's the caller's job.
 *
 * The 'ext' pointer is accepted in uninitialized form, and an
 * initialized structure is always returned, so that the caller may
 * free it even when an error return is given.
 *
 * Returns -
 * 0 OK
 * -1 FAIL
 */
RT_EXPORT extern int rt_db_cvt_to_external5(struct bu_external *ext,
					    const char *name,
					    const struct rt_db_internal *ip,
					    double conv2mm,
					    struct db_i *dbip,
					    struct resource *resp,
					    const int major);


/*
 * Modify name of external object, if necessary.
 */
RT_EXPORT extern int db_wrap_v5_external(struct bu_external *ep,
					 const char *name);



/**
 * Given an external representation of a database object, convert
 * it into its internal representation.
 *
 * Returns -
 * <0 On error
 * id On success.
 */
RT_EXPORT extern int
rt_db_external5_to_internal5(
    struct rt_db_internal *ip,
    const struct bu_external *ep,
    const char *name,
    const struct db_i *dbip,
    const mat_t mat,
    struct resource *resp);

/**
 * Get an object from the database, and convert it into its internal
 * representation.
 *
 * Applications and middleware shouldn't call this directly, they
 * should use the generic interface "rt_db_get_internal()".
 *
 * Returns -
 * <0 On error
 * id On success.
 */
RT_EXPORT extern int rt_db_get_internal5(struct rt_db_internal	*ip,
					 const struct directory	*dp,
					 const struct db_i	*dbip,
					 const mat_t		mat,
					 struct resource		*resp);


/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.
 *
 * Applications and middleware shouldn't call this directly, they
 * should use the version-generic interface "rt_db_put_internal()".
 *
 * The internal representation is always freed.  (Not the pointer,
 * just the contents).
 *
 * Returns -
 * <0 error
 * 0 success
 */
RT_EXPORT extern int rt_db_put_internal5(struct directory	*dp,
					 struct db_i		*dbip,
					 struct rt_db_internal	*ip,
					 struct resource		*resp,
					 const int		major);


/**
 * Make only the front (header) portion of a free object.  This is
 * used when operating on very large contiguous free objects in the
 * database (e.g. 50 MBytes).
 */
RT_EXPORT extern void db5_make_free_object_hdr(struct bu_external *ep,
					       size_t length);


/**
 * Make a complete, zero-filled, free object.  Note that free objects
 * can sometimes get quite large.
 */
RT_EXPORT extern void db5_make_free_object(struct bu_external *ep,
					   size_t length);


/**
 * Given a variable-width length field character pointer (cp) in
 * network order (XDR), store it in *lenp.
 *
 * Format is typically expected to be one of:
 *   DB5HDR_WIDTHCODE_8BIT
 *   DB5HDR_WIDTHCODE_16BIT
 *   DB5HDR_WIDTHCODE_32BIT
 *   DB5HDR_WIDTHCODE_64BIT
 *
 * Returns -
 * The number of bytes of input that were decoded.
 */
RT_EXPORT extern size_t db5_decode_signed(size_t *lenp,
					  const unsigned char *cp,
					  int format);

/**
 * Given a variable-width length field in network order (XDR), store
 * it in *lenp.
 *
 * This routine processes unsigned values.
 *
 * Returns -
 * The number of bytes of input that were decoded.
 */
RT_EXPORT extern size_t db5_decode_length(size_t *lenp,
					  const unsigned char *cp,
					  int format);
#if defined(USE_BINARY_ATTRIBUTES)
/**
 * Given a pointer to a binary attribute, determine its length.
 */
RT_EXPORT extern void decode_binary_attribute(const size_t len,
					      const unsigned char *cp);
#endif

/**
 * Given a number to encode, decide which is the smallest encoding
 * format which will contain it.
 */
RT_EXPORT extern int db5_select_length_encoding(size_t len);


RT_EXPORT extern void db5_import_color_table(char *cp);

/**
 * Given a value and a variable-width format spec, store it in network
 * order.
 *
 * Returns -
 * pointer to next available byte.
 */
RT_EXPORT extern unsigned char *db5_encode_length(unsigned char *cp,
						  size_t val,
						  int format);

/**
 * Given a pointer to the memory for a serialized database object, get
 * a raw internal representation.
 *
 * Returns -
 * on success, pointer to next unused byte in 'ip' after object got;
 * NULL, on error.
 */
RT_EXPORT extern const unsigned char *db5_get_raw_internal_ptr(struct db5_raw_internal *rip,
							       const unsigned char *ip);


/**
 * Given a file pointer to an open geometry database positioned on a
 * serialized object, get a raw internal representation.
 *
 * Returns -
 * 0 on success
 * -1 on EOF
 * -2 on error
 */
RT_EXPORT extern int db5_get_raw_internal_fp(struct db5_raw_internal	*rip,
					     FILE			*fp);


/**
 * Verify that this is a valid header for a BRL-CAD v5 database.
 *
 * Returns -
 * 0 Not valid v5 header
 * 1 Valid v5 header
 */
RT_EXPORT extern int db5_header_is_valid(const unsigned char *hp);


/**
 * write an ident header (title and units) to the provided file
 * pointer.
 *
 * Returns -
 * 0 Success
 * -1 Error
 */
RT_EXPORT extern int db5_fwrite_ident(FILE *,
				      const char *,
				      double);


/**
 *
 * Given that caller already has an external representation of the
 * database object, update it to have a new name (taken from
 * dp->d_namep) in that external representation, and write the new
 * object into the database, obtaining different storage if the size
 * has changed.
 *
 * Changing the name on a v5 object is a relatively expensive
 * operation.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * This routine is used to efficiently support MGED's "cp" and "keep"
 * commands, which don't need to import and decompress objects just to
 * rename and copy them.
 *
 * Returns -
 * -1 error
 * 0 success
 */
RT_EXPORT extern int db_put_external5(struct bu_external *ep,
				      struct directory *dp,
				      struct db_i *dbip);

/**
 * As the v4 database does not really have the notion of "wrapping",
 * this function writes the object name into the proper place (a
 * standard location in all granules).
 */
RT_EXPORT extern void db_wrap_v4_external(struct bu_external *op,
					  const char *name);


/* db_io.c */
RT_EXPORT extern int db_write(struct db_i	*dbip,
			      const void *	addr,
			      size_t		count,
			      b_off_t		offset);

/**
 * Add name from dp->d_namep to external representation of solid, and
 * write it into a file.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * The 'name' field of the external representation is modified to
 * contain the desired name.  The 'ep' parameter cannot be const.
 *
 * THIS ROUTINE ONLY SUPPORTS WRITING V4 GEOMETRY.
 *
 * Returns -
 * <0 error
 * 0 OK
 *
 * NOTE: Callers of this should be using wdb_export_external()
 * instead.
 */
RT_EXPORT extern int db_fwrite_external(FILE			*fp,
					const char		*name,
					struct bu_external	*ep);

/* malloc & read records */

/**
 * Retrieve all records in the database pertaining to an object, and
 * place them in malloc()'ed storage, which the caller is responsible
 * for free()'ing.
 *
 * This loads the combination into a local record buffer.  This is in
 * external v4 format.
 *
 * Returns -
 * union record * - OK
 * (union record *)0 - FAILURE
 */
RT_EXPORT extern union record *db_getmrec(const struct db_i *,
					  const struct directory *dp);
/* get several records from db */

/**
 * Retrieve 'len' records from the database, "offset" granules into
 * this entry.
 *
 * Returns -
 * 0 OK
 * -1 FAILURE
 */
RT_EXPORT extern int db_get(const struct db_i *,
			    const struct directory *dp,
			    union record *where,
			    b_off_t offset,
			    size_t len);
/* put several records into db */

/**
 * Store 'len' records to the database, "offset" granules into this
 * entry.
 *
 * Returns:
 * 0 OK
 * non-0 FAILURE
 */
RT_EXPORT extern int db_put(struct db_i *,
			    const struct directory *dp,
			    union record *where,
			    b_off_t offset, size_t len);

/**
 * Obtains a object from the database, leaving it in external
 * (on-disk) format.
 *
 * The bu_external structure represented by 'ep' is initialized here,
 * the caller need not pre-initialize it.  On error, 'ep' is left
 * un-initialized and need not be freed, to simplify error recovery.
 * On success, the caller is responsible for calling
 * bu_free_external(ep);
 *
 * Returns -
 * -1 error
 * 0 success
 */
RT_EXPORT extern int db_get_external(struct bu_external *ep,
				     const struct directory *dp,
				     const struct db_i *dbip);

/**
 * Given that caller already has an external representation of the
 * database object, update it to have a new name (taken from
 * dp->d_namep) in that external representation, and write the new
 * object into the database, obtaining different storage if the size
 * has changed.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * This routine is used to efficiently support MGED's "cp" and "keep"
 * commands, which don't need to import objects just to rename and
 * copy them.
 *
 * Returns -
 * <0 error
 * 0 success
 */
RT_EXPORT extern int db_put_external(struct bu_external *ep,
				     struct directory *dp,
				     struct db_i *dbip);

/* db_scan.c */
/* read db (to build directory) */
RT_EXPORT extern int db_scan(struct db_i *,
			     int (*handler)(struct db_i *,
					    const char *name,
					    b_off_t addr,
					    size_t nrec,
					    int flags,
					    void *client_data),
			     int do_old_matter,
			     void *client_data);
/* update db unit conversions */
#define db_ident(a, b, c)		+++error+++

/**
 * Update the _GLOBAL object, which in v5 serves the place of the
 * "ident" header record in v4 as the place to stash global
 * information.  Since every database will have one of these things,
 * it's no problem to update it.
 *
 * Returns -
 * 0 Success
 * -1 Fatal Error
 */
RT_EXPORT extern int db_update_ident(struct db_i *dbip,
				     const char *title,
				     double local2mm);

/**
 * Create a header for a v5 database.
 *
 * This routine has the same calling sequence as db_fwrite_ident()
 * which makes a v4 database header.
 *
 * In the v5 database, two database objects must be created to match
 * the semantics of what was in the v4 header:
 *
 * First, a database header object.
 *
 * Second, create a specially named attribute-only object which
 * contains the attributes "title=" and "units=" with the values of
 * title and local2mm respectively.
 *
 * Note that the current working units are specified as a conversion
 * factor to millimeters because database dimensional values are
 * always stored as millimeters (mm).  The units conversion factor
 * only affects the display and conversion of input values.  This
 * helps prevent error accumulation and improves numerical stability
 * when calculations are made.
 *
 * This routine should only be used by db_create().  Everyone else
 * should use db5_update_ident().
 *
 * Returns -
 * 0 Success
 * -1 Fatal Error
 */
RT_EXPORT extern int db_fwrite_ident(FILE *fp,
				     const char *title,
				     double local2mm);

/**
 * Initialize conversion factors given the v4 database unit
 */
RT_EXPORT extern void db_conversions(struct db_i *,
				     int units);

/**
 * Given a string, return the V4 database code representing the user's
 * preferred editing units.  The v4 database format does not have many
 * choices.
 *
 * Returns -
 * -1 Not a legal V4 database code
 * # The V4 database code number
 */
RT_EXPORT extern int db_v4_get_units_code(const char *str);

/* db5_scan.c */

/**
 * A generic routine to determine the type of the database, (v4 or v5)
 * and to invoke the appropriate db_scan()-like routine to build the
 * in-memory directory.
 *
 * It is the caller's responsibility to close the database in case of
 * error.
 *
 * Called from rt_dirbuild() and other places directly where a
 * raytrace instance is not required.
 *
 * Returns -
 * 0 OK
 * -1 failure
 */
RT_EXPORT extern int db_dirbuild(struct db_i *dbip);
RT_EXPORT extern int db_dirbuild_inmem(struct db_i *dbip, const void *data, b_off_t data_size);
RT_EXPORT extern struct directory *db5_diradd(struct db_i *dbip,
					      const struct db5_raw_internal *rip,
					      b_off_t laddr,
					      void *client_data);

/**
 * Scan a v5 database, sending each object off to a handler.
 *
 * Returns -
 * 0 Success
 * -1 Fatal Error
 */
RT_EXPORT extern int db5_scan(struct db_i *dbip,
			      void (*handler)(struct db_i *,
					      const struct db5_raw_internal *,
					      b_off_t addr,
					      void *client_data),
			      void *client_data);
RT_EXPORT extern int db5_scan_inmem(struct db_i *dbip,
				    void (*handler)(struct db_i *,
						    const struct db5_raw_internal *,
						    b_off_t addr,
						    void *client_data),
				    void *client_data,
				    const void *data,
				    b_off_t data_size);

/**
 * obtain the database version for a given database instance.
 *
 * presently returns only a 4 or 5 accordingly.
 */
RT_EXPORT extern int db_version(struct db_i *dbip);
RT_EXPORT extern int db_version_inmem(struct db_i *dbip, const void *data, b_off_t data_size);


/* db_corrupt.c */

/**
 * Detect whether a given geometry database file seems to be corrupt
 * or invalid due to flipped endianness.  Only relevant for v4
 * geometry files that are binary-incompatible with the runtime
 * platform.
 *
 * Returns true if flipping the endian type fixes all combination
 * member matrices.
 */
RT_EXPORT extern int rt_db_flip_endian(struct db_i *dbip);


/**
 * "open" an in-memory-only database instance.  this initializes a
 * dbip for use, creating an inmem dbi_wdbp as the means to add
 * geometry to the directory (use wdb_export_external()).
 */
RT_EXPORT extern struct db_i * db_open_inmem(void);


/**
 * creates an in-memory-only database.  this is very similar to
 * db_open_inmem() with the exception that the this routine adds a
 * default _GLOBAL object.
 */
RT_EXPORT extern struct db_i * db_create_inmem(void);


/**
 * Transmogrify an existing directory entry to be an in-memory-only
 * one, stealing the external representation from 'ext'.
 */
RT_EXPORT extern void db_inmem(struct directory	*dp,
			       struct bu_external	*ext,
			       int		flags,
			       struct db_i	*dbip);

/* db_lookup.c */

/**
 * Return the number of "struct directory" nodes in the given
 * database.
 */
RT_EXPORT extern size_t db_directory_size(const struct db_i *dbip);

/**
 * For debugging, ensure that all the linked-lists for the directory
 * structure are intact.
 */
RT_EXPORT extern void db_ck_directory(const struct db_i *dbip);

/**
 * Returns -
 * 0 if the in-memory directory is empty
 * 1 if the in-memory directory has entries,
 * which implies that a db_scan() has already been performed.
 */
RT_EXPORT extern int db_is_directory_non_empty(const struct db_i	*dbip);

/**
 * Returns a hash index for a given string that corresponds with the
 * head of that string's hash chain.
 */
RT_EXPORT extern int db_dirhash(const char *str);

/**
 * This routine ensures that ret_name is not already in the
 * directory. If it is, it tries a fixed number of times to modify
 * ret_name before giving up. Note - most of the time, the hash for
 * ret_name is computed once.
 *
 * Inputs -
 * dbip database instance pointer
 * ret_name the original name
 * noisy to blather or not
 *
 * Outputs -
 * ret_name the name to use
 * headp pointer to the first (struct directory *) in the bucket
 *
 * Returns -
 * 0 success
 * <0 fail
 */
RT_EXPORT extern int db_dircheck(struct db_i *dbip,
				 struct bu_vls *ret_name,
				 int noisy,
				 struct directory ***headp);
/* convert name to directory ptr */

/**
 * This routine takes a path or a name and returns the current directory
 * pointer (if any) associated with the object.
 *
 * If given an object name, it will look up the object name in the directory
 * table.  If the name is present, a pointer to the directory struct element is
 * returned, otherwise NULL is returned.
 *
 * If given a path, it will validate that the path is a valid path in the
 * current database.  If it is, a pointer to the current directory in the path
 * (i.e. the leaf object on the path) is returned, otherwise NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only the return code indicates
 * failure.
 *
 * Returns -
 * struct directory if name is found
 * RT_DIR_NULL on failure
 */
RT_EXPORT extern struct directory *db_lookup(const struct db_i *,
					     const char *name,
					     int noisy);

/* add entry to directory */

/**
 * Add an entry to the directory.  Try to make the regular path
 * through the code as fast as possible, to speed up building the
 * table of contents.
 *
 * dbip is a pointer to a valid/opened database instance
 *
 * name is the string name of the object being added
 *
 * laddr is the offset into the file to the object
 *
 * len is the length of the object, number of db granules used
 *
 * flags are defined in raytrace.h (RT_DIR_SOLID, RT_DIR_COMB, RT_DIR_REGION,
 * RT_DIR_INMEM, etc.) for db version 5, ptr is the minor_type
 * (non-null pointer to valid unsigned char code)
 *
 * an laddr of RT_DIR_PHONY_ADDR means that database storage has not
 * been allocated yet.
 */
RT_EXPORT extern struct directory *db_diradd(struct db_i *,
					     const char *name,
					     b_off_t laddr,
					     size_t len,
					     int flags,
					     void *ptr);
RT_EXPORT extern struct directory *db_diradd5(struct db_i *dbip,
					      const char *name,
					      b_off_t				laddr,
					      unsigned char			major_type,
					      unsigned char 			minor_type,
					      unsigned char			name_hidden,
					      size_t				object_length,
					      struct bu_attribute_value_set	*avs);

/* delete entry from directory */

/**
 * Given a pointer to a directory entry, remove it from the linked
 * list, and free the associated memory.
 *
 * It is the responsibility of the caller to have released whatever
 * structures have been hung on the d_use_hd bu_list, first.
 *
 * Returns -
 * 0 on success
 * non-0 on failure
 */
RT_EXPORT extern int db_dirdelete(struct db_i *,
				  struct directory *dp);
RT_EXPORT extern int db_fwrite_ident(FILE *,
				     const char *,
				     double);

/**
 * For debugging, print the entire contents of the database directory.
 */
RT_EXPORT extern void db_pr_dir(const struct db_i *dbip);

/**
 * Change the name string of a directory entry.  Because of the
 * hashing function, this takes some extra work.
 *
 * Returns -
 * 0 on success
 * non-0 on failure
 */
RT_EXPORT extern int db_rename(struct db_i *,
			       struct directory *,
			       const char *newname);


/**
 * Updates the d_nref fields (which count the number of times a given
 * entry is referenced by a COMBination in the database).
 *
 */
RT_EXPORT extern void db_update_nref(struct db_i *dbip,
				     struct resource *resp);



/* db_flags.c */
/**
 * Given the internal form of a database object, return the
 * appropriate 'flags' word for stashing in the in-memory directory of
 * objects.
 */
RT_EXPORT extern int db_flags_internal(const struct rt_db_internal *intern);


/* XXX - should use in db5_diradd() */
/**
 * Given a database object in "raw" internal form, return the
 * appropriate 'flags' word for stashing in the in-memory directory of
 * objects.
 */
RT_EXPORT extern int db_flags_raw_internal(const struct db5_raw_internal *intern);

/* db_alloc.c */

/* allocate "count" granules */
RT_EXPORT extern int db_alloc(struct db_i *,
			      struct directory *dp,
			      size_t count);
/* delete "recnum" from entry */
RT_EXPORT extern int db_delrec(struct db_i *,
			       struct directory *dp,
			       int recnum);
/* delete all granules assigned dp */
RT_EXPORT extern int db_delete(struct db_i *,
			       struct directory *dp);
/* write FREE records from 'start' */
RT_EXPORT extern int db_zapper(struct db_i *,
			       struct directory *dp,
			       size_t start);

/**
 * This routine is called by the RT_GET_DIRECTORY macro when the
 * freelist is exhausted.  Rather than simply getting one additional
 * structure, we get a whole batch, saving overhead.
 */
RT_EXPORT extern void db_alloc_directory_block(struct resource *resp);

/**
 * This routine is called by the GET_SEG macro when the freelist is
 * exhausted.  Rather than simply getting one additional structure, we
 * get a whole batch, saving overhead.  When this routine is called,
 * the seg resource must already be locked.  malloc() locking is done
 * in bu_malloc.
 */
RT_EXPORT extern void rt_alloc_seg_block(struct resource *res);



/**
 * Read named MGED db, build toc.
 */
RT_EXPORT extern struct rt_i *rt_dirbuild(const char *filename, char *buf, int len);
RT_EXPORT extern struct rt_i *rt_dirbuild_inmem(const void *data, b_off_t data_size, char *buf, int len);


/* db5_types.c */
RT_EXPORT extern int db5_type_tag_from_major(char	**tag,
					     const int	major);

RT_EXPORT extern int db5_type_descrip_from_major(char	**descrip,
						 const int	major);

RT_EXPORT extern int db5_type_tag_from_codes(char	**tag,
					     const int	major,
					     const int	minor);

RT_EXPORT extern int db5_type_descrip_from_codes(char	**descrip,
						 const int	major,
						 const int	minor);

RT_EXPORT extern int db5_type_codes_from_tag(int	*major,
					     int	*minor,
					     const char	*tag);

RT_EXPORT extern int db5_type_codes_from_descrip(int	*major,
						 int	*minor,
						 const char	*descrip);

RT_EXPORT extern size_t db5_type_sizeof_h_binu(const int minor);

RT_EXPORT extern size_t db5_type_sizeof_n_binu(const int minor);

__END_DECLS

#endif /* RT_DB_IO_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
