/*                      R A Y T R A C E . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2014 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file raytrace.h
 *
 * All the data structures and manifest constants necessary for
 * interacting with the BRL-CAD LIBRT ray-tracing library.
 *
 * Note that this header file defines many internal data structures,
 * as well as the library's external (interface) data structures.
 * These are provided for the convenience of applications builders.
 * However, the internal data structures are subject to change in each
 * release.
 *
 */

/* TODO - put together a dot file mapping the relationships between
 * high level rt structures and include it in the doxygen comments
 * with the \dotfile command:
 * http://www.stack.nl/~dimitri/doxygen/manual/commands.html#cmddotfile*/

#ifndef RAYTRACE_H
#define RAYTRACE_H

#include "common.h"

/* interface headers */
#include "tcl.h"
#include "bu/avs.h"
#include "bu/bitv.h"
#include "bu/bu_tcl.h"
#include "bu/file.h"
#include "bu/hash.h"
#include "bu/hist.h"
#include "bu/malloc.h"
#include "bu/mapped_file.h"
#include "bu/list.h"
#include "bu/log.h"
#include "bu/parallel.h" /* needed for BU_SEM_LAST */
#include "bu/parse.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bn.h"
#include "db5.h"
#include "nmg.h"
#include "pc.h"
#include "rtgeom.h"


#include "rt/defines.h"
#include "rt/db_fullpath.h"


__BEGIN_DECLS

#include "rt/debug.h"

#include "rt/tol.h"

#include "rt/db_internal.h"

#include "rt/xray.h"

#include "rt/hit.h"

#include "rt/seg.h"

#include "rt/soltab.h"

#include "rt/mater.h"

#include "rt/region.h"

#include "rt/ray_partition.h"

#include "rt/space_partition.h"

#include "rt/mem.h"

#include "rt/db_instance.h"

#include "rt/directory.h"

#include "rt/nongeom.h"

#include "rt/tree.h"

#include "rt/wdb.h"

#include "rt/anim.h"

#include "rt/piece.h"

#include "rt/resource.h"

#include "rt/application.h"

#include "rt/global.h"

#include "rt/rt_instance.h"

/**
 * Used by MGED for labeling vertices of a solid.
 *
 * TODO - eventually this should fade into a general annotation
 * framework
 */
struct rt_point_labels {
    char str[8];
    point_t pt;
};


#include "rt/view.h"

#include "rt/functab.h"

#include "rt/private.h"

#include "rt/nmg.h"

/*****************************************************************
 *                                                               *
 *          Applications interface to the RT library             *
 *                                                               *
 *****************************************************************/

#include "rt/overlap.h"

#include "rt/pattern.h"

#include "rt/shoot.h"

#include "rt/timer.h"


/* apply a matrix transformation */
/**
 * apply a matrix transformation to a given input object, setting the
 * resultant transformed object as the output solid.  if freeflag is
 * set, the input object will be released.
 *
 * returns zero if matrix transform was applied, non-zero on failure.
 */
RT_EXPORT extern int rt_matrix_transform(struct rt_db_internal *output, const mat_t matrix, struct rt_db_internal *input, int free_input, struct db_i *dbip, struct resource *resource);

#include "rt/boolweave.h"

/* find RPP of one region */

/**
 * Calculate the bounding RPP for a region given the name of the
 * region node in the database.  See remarks in _rt_getregion() above
 * for name conventions.  Returns 0 for failure (and prints a
 * diagnostic), or 1 for success.
 */
RT_EXPORT extern int rt_rpp_region(struct rt_i *rtip,
				   const char *reg_name,
				   fastf_t *min_rpp,
				   fastf_t *max_rpp);

/**
 * Compute the intersections of a ray with a rectangular
 * parallelepiped (RPP) that has faces parallel to the coordinate
 * planes
 *
 * The algorithm here was developed by Gary Kuehl for GIFT.  A good
 * description of the approach used can be found in "??" by XYZZY and
 * Barsky, ACM Transactions on Graphics, Vol 3 No 1, January 1984.
 *
 * Note: The computation of entry and exit distance is mandatory, as
 * the final test catches the majority of misses.
 *
 * Note: A hit is returned if the intersect is behind the start point.
 *
 * Returns -
 * 0 if ray does not hit RPP,
 * !0 if ray hits RPP.
 *
 * Implicit return -
 * rp->r_min = dist from start of ray to point at which ray ENTERS solid
 * rp->r_max = dist from start of ray to point at which ray LEAVES solid
 */
RT_EXPORT extern int rt_in_rpp(struct xray *rp,
			       const fastf_t *invdir,
			       const fastf_t *min,
			       const fastf_t *max);


/**
 * Return pointer to cell 'n' along a given ray.  Used for debugging
 * of how space partitioning interacts with shootray.  Intended to
 * mirror the operation of rt_shootray().  The first cell is 0.
 */
RT_EXPORT extern const union cutter *rt_cell_n_on_ray(struct application *ap,
						      int n);

/*
 * The rtip->rti_CutFree list can not be freed directly because is
 * bulk allocated.  Fortunately, we have a list of all the
 * bu_malloc()'ed blocks.  This routine may be called before the first
 * frame is done, so it must be prepared for uninitialized items.
 */
RT_EXPORT extern void rt_cut_clean(struct rt_i *rtip);

/* Find the bounding box given a struct rt_db_internal : bbox.c */

/**
 *
 * Calculate the bounding RPP of the internal format passed in 'ip'.
 * The bounding RPP is returned in rpp_min and rpp_max in mm FIXME:
 * This function needs to be modified to eliminate the rt_gettree()
 * call and the related parameters. In that case calling code needs to
 * call another function before calling this function That function
 * must create a union tree with tr_a.tu_op=OP_SOLID. It can look as
 * follows : union tree * rt_comb_tree(const struct db_i *dbip, const
 * struct rt_db_internal *ip). The tree is set in the struct
 * rt_db_internal * ip argument.  Once a suitable tree is set in the
 * ip, then this function can be called with the struct rt_db_internal
 * * to return the BB properly without getting stuck during tree
 * traversal in rt_bound_tree()
 *
 * Returns -
 *  0 success
 * -1 failure, the model bounds could not be got
 *
 */
RT_EXPORT extern int rt_bound_internal(struct db_i *dbip,
				       struct directory *dp,
				       point_t rpp_min,
				       point_t rpp_max);

#include "rt/cmd.h"

/* The database library */

/* wdb.c */
/** @addtogroup wdb */
/** @{ */
/** @file librt/wdb.c
 *
 * Routines to allow libwdb to use librt's import/export interface,
 * rather than having to know about the database formats directly.
 *
 */
RT_EXPORT extern struct rt_wdb *wdb_fopen(const char *filename);


/**
 * Create a libwdb output stream destined for a disk file.  This will
 * destroy any existing file by this name, and start fresh.  The file
 * is then opened in the normal "update" mode and an in-memory
 * directory is built along the way, allowing retrievals and object
 * replacements as needed.
 *
 * Users can change the database title by calling: ???
 */
RT_EXPORT extern struct rt_wdb *wdb_fopen_v(const char *filename,
					    int version);


/**
 * Create a libwdb output stream destined for an existing BRL-CAD
 * database, already opened via a db_open() call.
 *
 * RT_WDB_TYPE_DB_DISK Add to on-disk database
 * RT_WDB_TYPE_DB_DISK_APPEND_ONLY Add to on-disk database, don't clobber existing names, use prefix
 * RT_WDB_TYPE_DB_INMEM Add to in-memory database only
 * RT_WDB_TYPE_DB_INMEM_APPEND_ONLY Ditto, but give errors if name in use.
 */
RT_EXPORT extern struct rt_wdb *wdb_dbopen(struct db_i *dbip,
					   int mode);


/**
 * Returns -
 *  0 and modified *internp;
 * -1 ft_import failure (from rt_db_get_internal)
 * -2 db_get_external failure (from rt_db_get_internal)
 * -3 Attempt to import from write-only (stream) file.
 * -4 Name not found in database TOC.
 *
 * NON-PARALLEL because of rt_uniresource
 */
RT_EXPORT extern int wdb_import(struct rt_wdb *wdbp,
				struct rt_db_internal *internp,
				const char *name,
				const mat_t mat);


/**
 * The caller must free "ep".
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_export_external(struct rt_wdb *wdbp,
					 struct bu_external *ep,
					 const char *name,
					 int flags,
					 unsigned char minor_type);


/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.
 *
 * The internal representation is always freed.  This is the analog of
 * rt_db_put_internal() for rt_wdb objects.
 *
 * Use this routine in preference to wdb_export() whenever the caller
 * already has an rt_db_internal structure handy.
 *
 * NON-PARALLEL because of rt_uniresource
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_put_internal(struct rt_wdb *wdbp,
				      const char *name,
				      struct rt_db_internal *ip,
				      double local2mm);


/**
 * Export an in-memory representation of an object, as described in
 * the file h/rtgeom.h, into the indicated database.
 *
 * The internal representation (gp) is always freed.
 *
 * WARNING: The caller must be careful not to double-free gp,
 * particularly if it's been extracted from an rt_db_internal, e.g. by
 * passing intern.idb_ptr for gp.
 *
 * If the caller has an rt_db_internal structure handy already, they
 * should call wdb_put_internal() directly -- this is a convenience
 * routine intended primarily for internal use in LIBWDB.
 *
 * Returns -
 *  0 OK
 * <0 error
 */
RT_EXPORT extern int wdb_export(struct rt_wdb *wdbp,
				const char *name,
				void *gp,
				int id,
				double local2mm);
RT_EXPORT extern void wdb_init(struct rt_wdb *wdbp,
			       struct db_i   *dbip,
			       int           mode);


/**
 * Release from associated database "file", destroy dynamic data
 * structure.
 */
RT_EXPORT extern void wdb_close(struct rt_wdb *wdbp);


/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling.
 *
 * Returns -
 * BRLCAD_OK
 * BRLCAD_ERROR
 */
RT_EXPORT extern int wdb_import_from_path(struct bu_vls *logstr,
					  struct rt_db_internal *ip,
					  const char *path,
					  struct rt_wdb *wdb);


/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling. Additionally,
 * copies ts.ts_mat to matp.
 *
 * Returns -
 * BRLCAD_OK
 * BRLCAD_ERROR
 */
RT_EXPORT extern int wdb_import_from_path2(struct bu_vls *logstr,
					   struct rt_db_internal *ip,
					   const char *path,
					   struct rt_wdb *wdb,
					   matp_t matp);

/** @} */

/* db_anim.c */
RT_EXPORT extern struct animate *db_parse_1anim(struct db_i     *dbip,
						int             argc,
						const char      **argv);


/**
 * A common parser for mged and rt.  Experimental.  Not the best name
 * for this.
 */
RT_EXPORT extern int db_parse_anim(struct db_i     *dbip,
				   int             argc,
				   const char      **argv);

/**
 * Add a user-supplied animate structure to the end of the chain of
 * such structures hanging from the directory structure of the last
 * node of the path specifier.  When 'root' is non-zero, this matrix
 * is located at the root of the tree itself, rather than an arc, and
 * is stored differently.
 *
 * In the future, might want to check to make sure that callers
 * directory references are in the right database (dbip).
 */
RT_EXPORT extern int db_add_anim(struct db_i *dbip,
				 struct animate *anp,
				 int root);

/**
 * Perform the one animation operation.  Leave results in form that
 * additional operations can be cascaded.
 *
 * Note that 'materp' may be a null pointer, signifying that the
 * region has already been finalized above this point in the tree.
 */
RT_EXPORT extern int db_do_anim(struct animate *anp,
				mat_t stack,
				mat_t arc,
				struct mater_info *materp);

/**
 * Release chain of animation structures
 *
 * An unfortunate choice of name.
 */
RT_EXPORT extern void db_free_anim(struct db_i *dbip);

/**
 * Writes 'count' bytes into at file offset 'offset' from buffer at
 * 'addr'.  A wrapper for the UNIX write() sys-call that takes into
 * account syscall semaphores, stdio-only machines, and in-memory
 * buffering.
 *
 * Returns -
 * 0 OK
 * -1 FAILURE
 */
/* should be HIDDEN */
RT_EXPORT extern void db_write_anim(FILE *fop,
				    struct animate *anp);

/**
 * Parse one "anim" type command into an "animate" structure.
 *
 * argv[1] must be the "a/b" path spec,
 * argv[2] indicates what is to be animated on that arc.
 */
RT_EXPORT extern struct animate *db_parse_1anim(struct db_i *dbip,
						int argc,
						const char **argv);


/**
 * Free one animation structure
 */
RT_EXPORT extern void db_free_1anim(struct animate *anp);

/* search.c */

#include "./rt/search.h"

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
 * Given a variable-width length field in network order (XDR), store
 * it in *lenp.
 *
 * This routine processes signed values.
 *
 * Returns -
 * The number of bytes of input that were decoded.
 */
RT_EXPORT extern int db5_decode_signed(size_t			*lenp,
				       const unsigned char	*cp,
				       int			format);

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

/**
 * Given a number to encode, decide which is the smallest encoding
 * format which will contain it.
 */
RT_EXPORT extern int db5_select_length_encoding(size_t len);


RT_EXPORT extern void db5_import_color_table(char *cp);

/**
 * Convert the on-disk encoding into a handy easy-to-use
 * bu_attribute_value_set structure.
 *
 * Take advantage of the readonly_min/readonly_max capability so that
 * we don't have to bu_strdup() each string, but can simply point to
 * it in the provided buffer *ap.  Important implication: don't free
 * *ap until you're done with this avs.
 *
 * The upshot of this is that bu_avs_add() and bu_avs_remove() can be
 * safely used with this *avs.
 *
 * Returns -
 * >0 count of attributes successfully imported
 * -1 Error, mal-formed input
 */
RT_EXPORT extern int db5_import_attributes(struct bu_attribute_value_set *avs,
					   const struct bu_external *ap);

/**
 * Encode the attribute-value pair information into the external
 * on-disk format.
 *
 * The on-disk encoding is:
 *
 * name1 NULL value1 NULL ... nameN NULL valueN NULL NULL
 *
 * 'ext' is initialized on behalf of the caller.
 */
RT_EXPORT extern void db5_export_attributes(struct bu_external *ap,
					    const struct bu_attribute_value_set *avs);


/**
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

RT_EXPORT extern int db5_fwrite_ident(FILE *,
				      const char *,
				      double);


/**
 * Put the old region-id-color-table into the global object.  A null
 * attribute is set if the material table is empty.
 *
 * Returns -
 * <0 error
 * 0 OK
 */
RT_EXPORT extern int db5_put_color_table(struct db_i *dbip);
RT_EXPORT extern int db5_update_ident(struct db_i *dbip,
				      const char *title,
				      double local2mm);

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
 * Update an arbitrary number of attributes on a given database
 * object.  For efficiency, this is done without looking at the object
 * body at all.
 *
 * Contents of the bu_attribute_value_set are freed, but not the
 * struct itself.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_update_attributes(struct directory *dp,
					   struct bu_attribute_value_set *avsp,
					   struct db_i *dbip);

/**
 * A convenience routine to update the value of a single attribute.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_update_attribute(const char *obj_name,
					  const char *aname,
					  const char *value,
					  struct db_i *dbip);

/**
 * Replace the attributes of a given database object.
 *
 * For efficiency, this is done without looking at the object body at
 * all.  Contents of the bu_attribute_value_set are freed, but not the
 * struct itself.
 *
 * Returns -
 * 0 on success
 * <0 on error
 */
RT_EXPORT extern int db5_replace_attributes(struct directory *dp,
					    struct bu_attribute_value_set *avsp,
					    struct db_i *dbip);

/**
 *
 * Get attributes for an object pointed to by *dp
 *
 * returns:
 * 0 - all is well
 * <0 - error
 */
RT_EXPORT extern int db5_get_attributes(const struct db_i *dbip,
					struct bu_attribute_value_set *avs,
					const struct directory *dp);

/* db_comb.c */

/**
 * Return count of number of leaf nodes in this tree.
 */
RT_EXPORT extern size_t db_tree_nleaves(const union tree *tp);

/**
 * Take a binary tree in "V4-ready" layout (non-unions pushed below
 * unions, left-heavy), and flatten it into an array layout, ready for
 * conversion back to the GIFT-inspired V4 database format.
 *
 * This is done using the db_non_union_push() routine.
 *
 * If argument 'free' is non-zero, then the non-leaf nodes are freed
 * along the way, to prevent memory leaks.  In this case, the caller's
 * copy of 'tp' will be invalid upon return.
 *
 * When invoked at the very top of the tree, the op argument must be
 * OP_UNION.
 */
RT_EXPORT extern struct rt_tree_array *db_flatten_tree(struct rt_tree_array *rt_tree_array, union tree *tp, int op, int avail, struct resource *resp);

/**
 * Import a combination record from a V4 database into internal form.
 */
RT_EXPORT extern int rt_comb_import4(struct rt_db_internal	*ip,
				     const struct bu_external	*ep,
				     const mat_t		matrix,		/* NULL if identity */
				     const struct db_i		*dbip,
				     struct resource		*resp);

RT_EXPORT extern int rt_comb_export4(struct bu_external			*ep,
				     const struct rt_db_internal	*ip,
				     double				local2mm,
				     const struct db_i			*dbip,
				     struct resource			*resp);

/**
 * Produce a GIFT-compatible listing, one "member" per line,
 * regardless of the structure of the tree we've been given.
 */
RT_EXPORT extern void db_tree_flatten_describe(struct bu_vls	*vls,
					       const union tree	*tp,
					       int		indented,
					       int		lvl,
					       double		mm2local,
					       struct resource	*resp);

RT_EXPORT extern void db_tree_describe(struct bu_vls	*vls,
				       const union tree	*tp,
				       int		indented,
				       int		lvl,
				       double		mm2local);

RT_EXPORT extern void db_comb_describe(struct bu_vls	*str,
				       const struct rt_comb_internal	*comb,
				       int		verbose,
				       double		mm2local,
				       struct resource	*resp);

/**
 * OBJ[ID_COMBINATION].ft_describe() method
 */
RT_EXPORT extern int rt_comb_describe(struct bu_vls	*str,
				      const struct rt_db_internal *ip,
				      int		verbose,
				      double		mm2local,
				      struct resource *resp,
				      struct db_i *db_i);

/*==================== END g_comb.c / table.c interface ========== */

/**
 * As the v4 database does not really have the notion of "wrapping",
 * this function writes the object name into the proper place (a
 * standard location in all granules).
 */
RT_EXPORT extern void db_wrap_v4_external(struct bu_external *op,
					  const char *name);

/* Some export support routines */

/**
 * Support routine for db_ck_v4gift_tree().
 * Ensure that the tree below 'tp' is left-heavy, i.e. that there are
 * nothing but solids on the right side of any binary operations.
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
RT_EXPORT extern int db_ck_left_heavy_tree(const union tree	*tp,
					   int		no_unions);

/**
 * Look a gift-tree in the mouth.
 *
 * Ensure that this boolean tree conforms to the GIFT convention that
 * union operations must bind the loosest.
 *
 * There are two stages to this check:
 * 1) Ensure that if unions are present they are all at the root of tree,
 * 2) Ensure non-union children of union nodes are all left-heavy
 * (nothing but solid nodes permitted on rhs of binary operators).
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
RT_EXPORT extern int db_ck_v4gift_tree(const union tree *tp);

/**
 * Given a rt_tree_array array, build a tree of "union tree" nodes
 * appropriately connected together.  Every element of the
 * rt_tree_array array used is replaced with a TREE_NULL.  Elements
 * which are already TREE_NULL are ignored.  Returns a pointer to the
 * top of the tree.
 */
RT_EXPORT extern union tree *db_mkbool_tree(struct rt_tree_array *rt_tree_array,
					    size_t		howfar,
					    struct resource	*resp);

RT_EXPORT extern union tree *db_mkgift_tree(struct rt_tree_array *trees,
					    size_t subtreecount,
					    struct resource *resp);

/**
 * fills in rgb with the color for a given comb combination
 *
 * returns truthfully if a color could be got
 */
RT_EXPORT extern int rt_comb_get_color(unsigned char rgb[3], const struct rt_comb_internal *comb);

/* tgc.c */
RT_EXPORT extern void rt_pt_sort(fastf_t t[],
				 int npts);

/* ell.c */
RT_EXPORT extern void rt_ell_16pts(fastf_t *ov,
				   fastf_t *V,
				   fastf_t *A,
				   fastf_t *B);


/**
 * change all matching object names in the comb tree from old_name to
 * new_name
 *
 * calling function must supply an initialized bu_ptbl, and free it
 * once done.
 */
RT_EXPORT extern int db_comb_mvall(struct directory *dp,
				   struct db_i *dbip,
				   const char *old_name,
				   const char *new_name,
				   struct bu_ptbl *stack);

/* roots.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/roots.c
 *
 * Find the roots of a polynomial
 *
 */

/**
 * WARNING: The polynomial given as input is destroyed by this
 * routine.  The caller must save it if it is important!
 *
 * NOTE : This routine is written for polynomials with real
 * coefficients ONLY.  To use with complex coefficients, the Complex
 * Math library should be used throughout.  Some changes in the
 * algorithm will also be required.
 */
RT_EXPORT extern int rt_poly_roots(bn_poly_t *eqn,
				   bn_complex_t roots[],
				   const char *name);

/** @} */
/* db_io.c */
RT_EXPORT extern int db_write(struct db_i	*dbip,
			      const void *	addr,
			      size_t		count,
			      off_t		offset);

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
			    off_t offset,
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
			    off_t offset, size_t len);

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
					    off_t addr,
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
RT_EXPORT extern struct directory *db5_diradd(struct db_i *dbip,
					      const struct db5_raw_internal *rip,
					      off_t laddr,
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
					      off_t addr,
					      void *client_data),
			      void *client_data);

/**
 * obtain the database version for a given database instance.
 *
 * presently returns only a 4 or 5 accordingly.
 */
RT_EXPORT extern int db_version(struct db_i *dbip);


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


/* db5_comb.c */

/**
 * Read a combination object in v5 external (on-disk) format, and
 * convert it into the internal format described in rtgeom.h
 *
 * This is an unusual conversion, because some of the data is taken
 * from attributes, not just from the object body.  By the time this
 * is called, the attributes will already have been cracked into
 * ip->idb_avs, we get the attributes from there.
 *
 * Returns -
 * 0 OK
 * -1 FAIL
 */
RT_EXPORT extern int rt_comb_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);

/* extrude.c */
RT_EXPORT extern int rt_extrude_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip, struct resource *resp);


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
 * This routine takes a name and looks it up in the directory table.
 * If the name is present, a pointer to the directory struct element
 * is returned, otherwise NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only the return code
 * indicates failure.
 *
 * Returns -
 * struct directory if name is found
 * RT_DIR_NULL on failure
 */
RT_EXPORT extern struct directory *db_lookup(const struct db_i *,
					     const char *name,
					     int noisy);
/* lookup directory entries based on attributes */

/**
 * lookup directory entries based on directory flags (dp->d_flags) and
 * attributes the "dir_flags" arg is a mask for the directory flags
 * the *"avs" is an attribute value set used to select from the
 * objects that *pass the flags mask. if "op" is 1, then the object
 * must have all the *attributes and values that appear in "avs" in
 * order to be *selected. If "op" is 2, then the object must have at
 * least one of *the attribute/value pairs from "avs".
 *
 * dir_flags are in the form used in struct directory (d_flags)
 *
 * for op:
 * 1 -> all attribute name/value pairs must be present and match
 * 2 -> at least one of the name/value pairs must be present and match
 *
 * returns a ptbl list of selected directory pointers an empty list
 * means nothing met the requirements a NULL return means something
 * went wrong.
 */
RT_EXPORT extern struct bu_ptbl *db_lookup_by_attr(struct db_i *dbip,
						   int dir_flags,
						   struct bu_attribute_value_set *avs,
						   int op);

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
					     off_t laddr,
					     size_t len,
					     int flags,
					     void *ptr);
RT_EXPORT extern struct directory *db_diradd5(struct db_i *dbip,
					      const char *name,
					      off_t				laddr,
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


/**
 * DEPRECATED: Use db_ls() instead of this function.
 *
 * Appends a list of all database matches to the given vls, or the
 * pattern itself if no matches are found.  Returns the number of
 * matches.
 */
DEPRECATED RT_EXPORT extern int db_regexp_match_all(struct bu_vls *dest,
					 struct db_i *dbip,
					 const char *pattern);

/* db_ls.c */
/**
 * db_ls takes a database instance pointer and assembles a directory
 * pointer array of objects in the database according to a set of
 * flags.  An optional pattern can be supplied for match filtering
 * via globbing rules (see bu_fnmatch).  If pattern is NULL, filtering
 * is performed using only the flags.
 *
 * The caller is responsible for freeing the array.
 *
 * Returns -
 * integer count of objects in dpv
 * struct directory ** array of objects in dpv via argument
 *
 */
#define DB_LS_PRIM         0x1    /* filter for primitives (solids)*/
#define DB_LS_COMB         0x2    /* filter for combinations */
#define DB_LS_REGION       0x4    /* filter for regions */
#define DB_LS_HIDDEN       0x8    /* include hidden objects in results */
#define DB_LS_NON_GEOM     0x10   /* filter for non-geometry objects */
#define DB_LS_TOPS         0x20   /* filter for objects un-referenced by other objects */
/* TODO - implement this flag
#define DB_LS_REGEX	   0x40*/ /* interpret pattern using regex rules, instead of
				     globbing rules (default) */
RT_EXPORT extern size_t db_ls(const struct db_i *dbip,
			      int flags,
			      const char *pattern,
			      struct directory ***dpv);

/**
 * convert an argv list of names to a directory pointer array.
 *
 * If db_lookup fails for any individual argv, an empty directory
 * structure is created and assigned the name and RT_DIR_PHONY_ADDR
 *
 * The returned directory ** structure is NULL terminated.
 */
RT_EXPORT extern struct directory **db_argv_to_dpv(const struct db_i *dbip,
						   const char **argv);


/**
 * convert a directory pointer array to an argv char pointer array.
 */
RT_EXPORT extern char **db_dpv_to_argv(struct directory **dpv);


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
 * 'arc' may be a null pointer, signifying an identity matrix.
 * 'materp' may be a null pointer, signifying that the region has
 * already been finalized above this point in the tree.
 */
RT_EXPORT extern void db_apply_anims(struct db_full_path *pathp,
				     struct directory *dp,
				     mat_t stck,
				     mat_t arc,
				     struct mater_info *materp);

/**
 * Given the name of a region, return the matrix which maps model
 * coordinates into "region" coordinates.
 *
 * Returns:
 * 0 OK
 * <0 Failure
 */
RT_EXPORT extern int db_region_mat(mat_t		m,		/* result */
				   struct db_i	*dbip,
				   const char	*name,
				   struct resource *resp);


/**
 * XXX given that this routine depends on rtip, it should be called
 * XXX rt_shader_mat().
 *
 * Given a region, return a matrix which maps model coordinates into
 * region "shader space".  This is a space where points in the model
 * within the bounding box of the region are mapped into "region"
 * space (the coordinate system in which the region is defined).  The
 * area occupied by the region's bounding box (in region coordinates)
 * are then mapped into the unit cube.  This unit cube defines "shader
 * space".
 *
 * Returns:
 * 0 OK
 * <0 Failure
 */
RT_EXPORT extern int rt_shader_mat(mat_t			model_to_shader,	/* result */
				   const struct rt_i	*rtip,
				   const struct region	*rp,
				   point_t			p_min,	/* input/output: shader/region min point */
				   point_t			p_max,	/* input/output: shader/region max point */
				   struct resource		*resp);


/* dir.c */

/**
 * Read named MGED db, build toc.
 */
RT_EXPORT extern struct rt_i *rt_dirbuild(const char *filename, char *buf, int len);

/**
 * Get an object from the database, and convert it into its internal
 * representation.
 *
 * Returns -
 * <0 On error
 * id On success.
 */
RT_EXPORT extern int rt_db_get_internal(struct rt_db_internal	*ip,
					const struct directory	*dp,
					const struct db_i	*dbip,
					const mat_t		mat,
					struct resource		*resp);

/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.  On success only, the internal
 * representation is freed.
 *
 * Returns -
 * <0 error
 * 0 success
 */
RT_EXPORT extern int rt_db_put_internal(struct directory	*dp,
					struct db_i		*dbip,
					struct rt_db_internal	*ip,
					struct resource		*resp);

/**
 * Put an object in internal format out onto a file in external
 * format.  Used by LIBWDB.
 *
 * Can't really require a dbip parameter, as many callers won't have
 * one.
 *
 * THIS ROUTINE ONLY SUPPORTS WRITING V4 GEOMETRY.
 *
 * Returns -
 * 0 OK
 * <0 error
 */
RT_EXPORT extern int rt_fwrite_internal(FILE *fp,
					const char *name,
					const struct rt_db_internal *ip,
					double conv2mm);
RT_EXPORT extern void rt_db_free_internal(struct rt_db_internal *ip);


/**
 * Convert an object name to a rt_db_internal pointer
 *
 * Looks up the named object in the directory of the specified model,
 * obtaining a directory pointer.  Then gets that object from the
 * database and constructs its internal representation.  Returns
 * ID_NULL on error, otherwise returns the type of the object.
 */
RT_EXPORT extern int rt_db_lookup_internal(struct db_i *dbip,
					   const char *obj_name,
					   struct directory **dpp,
					   struct rt_db_internal *ip,
					   int noisy,
					   struct resource *resp);

RT_EXPORT extern void rt_optim_tree(union tree *tp,
				    struct resource *resp);


/* mirror.c */
RT_EXPORT extern struct rt_db_internal *rt_mirror(struct db_i *dpip,
						  struct rt_db_internal *ip,
						  point_t mirror_pt,
						  vect_t mirror_dir,
						  struct resource *resp);

/*
  RT_EXPORT extern void db_preorder_traverse(struct directory *dp,
  struct db_traverse *dtp);
*/

/* arb8.c */
RT_EXPORT extern int rt_arb_get_cgtype(
    int *cgtype,
    struct rt_arb_internal *arb,
    const struct bn_tol *tol,
    register int *uvec,  /* array of indexes to unique points in arb->pt[] */
    register int *svec); /* array of indexes to like points in arb->pt[] */
RT_EXPORT extern int rt_arb_std_type(const struct rt_db_internal *ip,
				     const struct bn_tol *tol);
RT_EXPORT extern void rt_arb_centroid(point_t                       *cent,
				      const struct rt_db_internal   *ip);
RT_EXPORT extern int rt_arb_calc_points(struct rt_arb_internal *arb, int cgtype, const plane_t planes[6], const struct bn_tol *tol);		/* needs wdb.h for arg list */
RT_EXPORT extern int rt_arb_check_points(struct rt_arb_internal *arb,
					 int cgtype,
					 const struct bn_tol *tol);
RT_EXPORT extern int rt_arb_3face_intersect(point_t			point,
					    const plane_t		planes[6],
					    int			type,		/* 4..8 */
					    int			loc);
RT_EXPORT extern int rt_arb_calc_planes(struct bu_vls		*error_msg_ret,
					struct rt_arb_internal	*arb,
					int			type,
					plane_t			planes[6],
					const struct bn_tol	*tol);
RT_EXPORT extern int rt_arb_move_edge(struct bu_vls		*error_msg_ret,
				      struct rt_arb_internal	*arb,
				      vect_t			thru,
				      int			bp1,
				      int			bp2,
				      int			end1,
				      int			end2,
				      const vect_t		dir,
				      plane_t			planes[6],
				      const struct bn_tol	*tol);
RT_EXPORT extern int rt_arb_edit(struct bu_vls		*error_msg_ret,
				 struct rt_arb_internal	*arb,
				 int			arb_type,
				 int			edit_type,
				 vect_t			pos_model,
				 plane_t			planes[6],
				 const struct bn_tol	*tol);
RT_EXPORT extern int rt_arb_find_e_nearest_pt2(int *edge, int *vert1, int *vert2, const struct rt_db_internal *ip, const point_t pt2, const mat_t mat, fastf_t ptol);

/* epa.c */
RT_EXPORT extern void rt_ell(fastf_t *ov,
			     const fastf_t *V,
			     const fastf_t *A,
			     const fastf_t *B,
			     int sides);

/* pipe.c */
RT_EXPORT extern void rt_vls_pipept(struct bu_vls *vp,
				    int seg_no,
				    const struct rt_db_internal *ip,
				    double mm2local);
RT_EXPORT extern void rt_pipept_print(const struct wdb_pipept *pipept, double mm2local);
RT_EXPORT extern int rt_pipe_ck(const struct bu_list *headp);

/* metaball.c */
struct rt_metaball_internal;
RT_EXPORT extern void rt_vls_metaballpt(struct bu_vls *vp,
					const int pt_no,
					const struct rt_db_internal *ip,
					const double mm2local);
RT_EXPORT extern void rt_metaballpt_print(const struct wdb_metaballpt *metaball, double mm2local);
RT_EXPORT extern int rt_metaball_ck(const struct bu_list *headp);
RT_EXPORT extern fastf_t rt_metaball_point_value(const point_t *p,
						 const struct rt_metaball_internal *mb);
RT_EXPORT extern int rt_metaball_point_inside(const point_t *p,
					      const struct rt_metaball_internal *mb);
RT_EXPORT extern int rt_metaball_lookup_type_id(const char *name);
RT_EXPORT extern const char *rt_metaball_lookup_type_name(const int id);
RT_EXPORT extern int rt_metaball_add_point(struct rt_metaball_internal *,
					   const point_t *loc,
					   const fastf_t fldstr,
					   const fastf_t goo);

/* rpc.c */
RT_EXPORT extern int rt_mk_parabola(struct rt_pt_node *pts,
				    fastf_t r,
				    fastf_t b,
				    fastf_t dtol,
				    fastf_t ntol);


RT_EXPORT extern struct bn_vlblock *rt_vlblock_init(void);


RT_EXPORT extern void rt_vlblock_free(struct bn_vlblock *vbp);

RT_EXPORT extern struct bu_list *rt_vlblock_find(struct bn_vlblock *vbp,
						 int r,
						 int g,
						 int b);

/* ars.c */
RT_EXPORT extern void rt_hitsort(struct hit h[],
				 int nh);

/* pg.c */
RT_EXPORT extern int rt_pg_to_bot(struct rt_db_internal *ip,
				  const struct bn_tol *tol,
				  struct resource *resp0);
RT_EXPORT extern int rt_pg_plot(struct bu_list		*vhead,
				struct rt_db_internal	*ip,
				const struct rt_tess_tol *ttol,
				const struct bn_tol	*tol,
				const struct rt_view_info *info);
RT_EXPORT extern int rt_pg_plot_poly(struct bu_list		*vhead,
				     struct rt_db_internal	*ip,
				     const struct rt_tess_tol	*ttol,
				     const struct bn_tol	*tol);

/* hf.c */
RT_EXPORT extern int rt_hf_to_dsp(struct rt_db_internal *db_intern);

/* dsp.c */
RT_EXPORT extern int dsp_pos(point_t out,
			     struct soltab *stp,
			     point_t p);

/* pr.c */
RT_EXPORT extern void rt_pr_fallback_angle(struct bu_vls *str,
					   const char *prefix,
					   const double angles[5]);
RT_EXPORT extern void rt_find_fallback_angle(double angles[5],
					     const vect_t vec);
RT_EXPORT extern void rt_pr_tol(const struct bn_tol *tol);

/* regionfix.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/regionfix.c
 *
 * Subroutines for adjusting old GIFT-style region-IDs, to take into
 * account the presence of instancing.
 *
 */

/* table.c */
RT_EXPORT extern int rt_id_solid(struct bu_external *ep);
RT_EXPORT extern const struct rt_functab *rt_get_functab_by_label(const char *label);
RT_EXPORT extern int rt_generic_xform(struct rt_db_internal	*op,
				      const mat_t		mat,
				      struct rt_db_internal	*ip,
				      int			avail,
				      struct db_i		*dbip,
				      struct resource		*resp);


/* prep.c */
RT_EXPORT extern void rt_plot_all_bboxes(FILE *fp,
					 struct rt_i *rtip);
RT_EXPORT extern void rt_plot_all_solids(FILE		*fp,
					 struct rt_i	*rtip,
					 struct resource	*resp);

RT_EXPORT extern int rt_find_paths(struct db_i *dbip,
				   struct directory *start,
				   struct directory *end,
				   struct bu_ptbl *paths,
				   struct resource *resp);

RT_EXPORT extern struct bu_bitv *rt_get_solidbitv(size_t nbits,
						  struct resource *resp);

#include "rt/prep.h"


/* vlist.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/vlist.c
 *
 * Routines for the import and export of vlist chains as:
 * Network independent binary,
 * BRL-extended UNIX-Plot files.
 *
 */

/* FIXME: Has some stuff mixed in here that should go in LIBBN */
/************************************************************************
 *									*
 *			Generic VLBLOCK routines			*
 *									*
 ************************************************************************/

RT_EXPORT extern struct bn_vlblock *bn_vlblock_init(struct bu_list	*free_vlist_hd,	/* where to get/put free vlists */
						    int		max_ent);

RT_EXPORT extern struct bn_vlblock *	rt_vlblock_init(void);


RT_EXPORT extern void rt_vlblock_free(struct bn_vlblock *vbp);

RT_EXPORT extern struct bu_list *rt_vlblock_find(struct bn_vlblock *vbp,
						 int r,
						 int g,
						 int b);


/************************************************************************
 *									*
 *			Generic BN_VLIST routines			*
 *									*
 ************************************************************************/

/**
 * Returns the description of a vlist cmd.
 */
RT_EXPORT extern const char *rt_vlist_get_cmd_description(int cmd);

/**
 * Validate an bn_vlist chain for having reasonable values inside.
 * Calls bu_bomb() if not.
 *
 * Returns -
 * npts Number of point/command sets in use.
 */
RT_EXPORT extern int rt_ck_vlist(const struct bu_list *vhead);


/**
 * Duplicate the contents of a vlist.  Note that the copy may be more
 * densely packed than the source.
 */
RT_EXPORT extern void rt_vlist_copy(struct bu_list *dest,
				    const struct bu_list *src);


/**
 * The macro RT_FREE_VLIST() simply appends to the list
 * &RTG.rtg_vlfree.  Now, give those structures back to bu_free().
 */
RT_EXPORT extern void bn_vlist_cleanup(struct bu_list *hd);


/**
 * XXX This needs to remain a LIBRT function.
 */
RT_EXPORT extern void rt_vlist_cleanup(void);



/************************************************************************
 *									*
 *			Binary VLIST import/export routines		*
 *									*
 ************************************************************************/

/**
 * Convert a vlist chain into a blob of network-independent binary,
 * for transmission to another machine.
 *
 * The result is stored in a vls string, so that both the address and
 * length are available conveniently.
 */
RT_EXPORT extern void rt_vlist_export(struct bu_vls *vls,
				      struct bu_list *hp,
				      const char *name);


/**
 * Convert a blob of network-independent binary prepared by
 * vls_vlist() and received from another machine, into a vlist chain.
 */
RT_EXPORT extern void rt_vlist_import(struct bu_list *hp,
				      struct bu_vls *namevls,
				      const unsigned char *buf);


/************************************************************************
 *									*
 *			UNIX-Plot VLIST import/export routines		*
 *									*
 ************************************************************************/

/**
 * Output a bn_vlblock object in extended UNIX-plot format, including
 * color.
 */
RT_EXPORT extern void rt_plot_vlblock(FILE *fp,
				      const struct bn_vlblock *vbp);


/**
 * Output a vlist as an extended 3-D floating point UNIX-Plot file.
 * You provide the file.  Uses libplot3 routines to create the
 * UNIX-Plot output.
 */
RT_EXPORT extern void rt_vlist_to_uplot(FILE *fp,
					const struct bu_list *vhead);
RT_EXPORT extern int rt_process_uplot_value(struct bu_list **vhead,
					    struct bn_vlblock *vbp,
					    FILE *fp,
					    int c,
					    double char_size,
					    int mode);


/**
 * Read a BRL-style 3-D UNIX-plot file into a vector list.  For now,
 * discard color information, only extract vectors.  This might be
 * more naturally located in mged/plot.c
 */
RT_EXPORT extern int rt_uplot_to_vlist(struct bn_vlblock *vbp,
				       FILE *fp,
				       double char_size,
				       int mode);


/**
 * Used by MGED's "labelvert" command.
 */
RT_EXPORT extern void rt_label_vlist_verts(struct bn_vlblock *vbp,
					   struct bu_list *src,
					   mat_t mat,
					   double sz,
					   double mm2local);
/** @} */
/* sketch.c */
RT_EXPORT extern int curve_to_vlist(struct bu_list		*vhead,
				    const struct rt_tess_tol	*ttol,
				    point_t			V,
				    vect_t			u_vec,
				    vect_t			v_vec,
				    struct rt_sketch_internal *sketch_ip,
				    struct rt_curve		*crv);

RT_EXPORT extern int rt_check_curve(const struct rt_curve *crv,
				    const struct rt_sketch_internal *skt,
				    int noisy);

RT_EXPORT extern void rt_curve_reverse_segment(uint32_t *lng);
RT_EXPORT extern void rt_curve_order_segments(struct rt_curve *crv);

RT_EXPORT extern void rt_copy_curve(struct rt_curve *crv_out,
				    const struct rt_curve *crv_in);

RT_EXPORT extern void rt_curve_free(struct rt_curve *crv);
RT_EXPORT extern void rt_copy_curve(struct rt_curve *crv_out,
				    const struct rt_curve *crv_in);
RT_EXPORT extern struct rt_sketch_internal *rt_copy_sketch(const struct rt_sketch_internal *sketch_ip);
RT_EXPORT extern int curve_to_tcl_list(struct bu_vls *vls,
				       struct rt_curve *crv);

/* htbl.c */

RT_EXPORT extern void rt_htbl_init(struct rt_htbl *b, size_t len, const char *str);

/**
 * Reset the table to have no elements, but retain any existing storage.
 */
RT_EXPORT extern void rt_htbl_reset(struct rt_htbl *b);

/**
 * Deallocate dynamic hit buffer and render unusable without a
 * subsequent rt_htbl_init().
 */
RT_EXPORT extern void rt_htbl_free(struct rt_htbl *b);

/**
 * Allocate another hit structure, extending the array if necessary.
 */
RT_EXPORT extern struct hit *rt_htbl_get(struct rt_htbl *b);


/* bot.c */
RT_EXPORT extern size_t rt_bot_get_edge_list(const struct rt_bot_internal *bot,
					     size_t **edge_list);
RT_EXPORT extern int rt_bot_edge_in_list(const size_t v1,
					 const size_t v2,
					 const size_t edge_list[],
					 const size_t edge_count0);
RT_EXPORT extern int rt_bot_plot(struct bu_list		*vhead,
				 struct rt_db_internal	*ip,
				 const struct rt_tess_tol *ttol,
				 const struct bn_tol	*tol,
				 const struct rt_view_info *info);
RT_EXPORT extern int rt_bot_plot_poly(struct bu_list		*vhead,
				      struct rt_db_internal	*ip,
				      const struct rt_tess_tol *ttol,
				      const struct bn_tol	*tol);
RT_EXPORT extern int rt_bot_find_v_nearest_pt2(const struct rt_bot_internal *bot,
					       const point_t	pt2,
					       const mat_t	mat);
RT_EXPORT extern int rt_bot_find_e_nearest_pt2(int *vert1, int *vert2, const struct rt_bot_internal *bot, const point_t pt2, const mat_t mat);
RT_EXPORT extern fastf_t rt_bot_propget(struct rt_bot_internal *bot,
					const char *property);
RT_EXPORT extern int rt_bot_vertex_fuse(struct rt_bot_internal *bot,
					const struct bn_tol *tol);
RT_EXPORT extern int rt_bot_face_fuse(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_condense(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_smooth(struct rt_bot_internal *bot,
				   const char *bot_name,
				   struct db_i *dbip,
				   fastf_t normal_tolerance_angle);
RT_EXPORT extern int rt_bot_flip(struct rt_bot_internal *bot);
RT_EXPORT extern int rt_bot_sync(struct rt_bot_internal *bot);
RT_EXPORT extern struct rt_bot_list * rt_bot_split(struct rt_bot_internal *bot);
RT_EXPORT extern struct rt_bot_list * rt_bot_patches(struct rt_bot_internal *bot);
RT_EXPORT extern void rt_bot_list_free(struct rt_bot_list *headRblp,
				       int fbflag);

RT_EXPORT extern int rt_bot_same_orientation(const int *a,
					     const int *b);

RT_EXPORT extern int rt_bot_tess(struct nmgregion **r,
				 struct model *m,
				 struct rt_db_internal *ip,
				 const struct rt_tess_tol *ttol,
				 const struct bn_tol *tol);

/* BREP drawing routines */
RT_EXPORT extern int rt_brep_plot(struct bu_list		*vhead,
				 struct rt_db_internal		*ip,
				 const struct rt_tess_tol	*ttol,
				 const struct bn_tol		*tol,
				 const struct rt_view_info *info);
RT_EXPORT extern int rt_brep_plot_poly(struct bu_list		*vhead,
					  const struct db_full_path *pathp,
				      struct rt_db_internal	*ip,
				      const struct rt_tess_tol	*ttol,
				      const struct bn_tol	*tol,
				      const struct rt_view_info *info);
/* BREP validity test */
RT_EXPORT extern int rt_brep_valid(struct rt_db_internal *ip, struct bu_vls *log);

/* torus.c */
RT_EXPORT extern int rt_num_circular_segments(double maxerr,
					      double radius);

/* tcl.c */
/** @addtogroup librt */
/** @{ */
/** @file librt/tcl.c
 *
 * Tcl interfaces to LIBRT routines.
 *
 * LIBRT routines are not for casual command-line use; as a result,
 * the Tcl name for the function should be exactly the same as the C
 * name for the underlying function.  Typing "rt_" is no hardship when
 * writing Tcl procs, and clarifies the origin of the routine.
 *
 */


/**
 * Allow specification of a ray to trace, in two convenient formats.
 *
 * Examples -
 * {0 0 0} dir {0 0 -1}
 * {20 -13.5 20} at {10 .5 3}
 */
RT_EXPORT extern int rt_tcl_parse_ray(Tcl_Interp *interp,
				      struct xray *rp,
				      const char *const*argv);


/**
 * Format a "union cutter" structure in a TCL-friendly format.  Useful
 * for debugging space partitioning
 *
 * Examples -
 * type cutnode
 * type boxnode
 * type nugridnode
 */
RT_EXPORT extern void rt_tcl_pr_cutter(Tcl_Interp *interp,
				       const union cutter *cutp);
RT_EXPORT extern int rt_tcl_cutter(ClientData clientData,
				   Tcl_Interp *interp,
				   int argc,
				   const char *const*argv);


/**
 * Format a hit in a TCL-friendly format.
 *
 * It is possible that a solid may have been removed from the
 * directory after this database was prepped, so check pointers
 * carefully.
 *
 * It might be beneficial to use some format other than %g to give the
 * user more precision.
 */
RT_EXPORT extern void rt_tcl_pr_hit(Tcl_Interp *interp, struct hit *hitp, const struct seg *segp, int flipflag);


/**
 * Generic interface for the LIBRT_class manipulation routines.
 *
 * Usage:
 * procname dbCmdName ?args?
 * Returns: result of cmdName LIBRT operation.
 *
 * Objects of type 'procname' must have been previously created by the
 * 'rt_gettrees' operation performed on a database object.
 *
 * Example -
 *	.inmem rt_gettrees .rt all.g
 *	.rt shootray {0 0 0} dir {0 0 -1}
 */
RT_EXPORT extern int rt_tcl_rt(ClientData clientData,
			       Tcl_Interp *interp,
			       int argc,
			       const char **argv);


/************************************************************************************************
 *												*
 *			Tcl interface to the Database						*
 *												*
 ************************************************************************************************/

/**
 * Given the name of a database object or a full path to a leaf
 * object, obtain the internal form of that leaf.  Packaged separately
 * mainly to make available nice Tcl error handling.
 *
 * Returns -
 * TCL_OK
 * TCL_ERROR
 */
RT_EXPORT extern int rt_tcl_import_from_path(Tcl_Interp *interp,
					     struct rt_db_internal *ip,
					     const char *path,
					     struct rt_wdb *wdb);
RT_EXPORT extern void rt_generic_make(const struct rt_functab *ftp, struct rt_db_internal *intern);


/**
 * Add all the supported Tcl interfaces to LIBRT routines to the list
 * of commands known by the given interpreter.
 *
 * wdb_open creates database "objects" which appear as Tcl procs,
 * which respond to many operations.
 *
 * The "db rt_gettrees" operation in turn creates a ray-traceable
 * object as yet another Tcl proc, which responds to additional
 * operations.
 *
 * Note that the MGED mainline C code automatically runs "wdb_open db"
 * and "wdb_open .inmem" on behalf of the MGED user, which exposes all
 * of this power.
 */
RT_EXPORT extern void rt_tcl_setup(Tcl_Interp *interp);
RT_EXPORT extern int Sysv_Init(Tcl_Interp *interp);


/**
 * Allows LIBRT to be dynamically loaded to a vanilla tclsh/wish with
 * "load /usr/brlcad/lib/libbu.so"
 * "load /usr/brlcad/lib/libbn.so"
 * "load /usr/brlcad/lib/librt.so"
 */
RT_EXPORT extern int Rt_Init(Tcl_Interp *interp);


/**
 * Take a db_full_path and append it to the TCL result string.
 *
 * NOT moving to db_fullpath.h because it is evil Tcl_Interp api
 */
RT_EXPORT extern void db_full_path_appendresult(Tcl_Interp *interp,
						const struct db_full_path *pp);


/**
 * Expects the Tcl_obj argument (list) to be a Tcl list and extracts
 * list elements, converts them to int, and stores them in the passed
 * in array. If the array_len argument is zero, a new array of
 * appropriate length is allocated. The return value is the number of
 * elements converted.
 */
RT_EXPORT extern int tcl_obj_to_int_array(Tcl_Interp *interp,
					  Tcl_Obj *list,
					  int **array,
					  int *array_len);


/**
 * Expects the Tcl_obj argument (list) to be a Tcl list and extracts
 * list elements, converts them to fastf_t, and stores them in the
 * passed in array. If the array_len argument is zero, a new array of
 * appropriate length is allocated. The return value is the number of
 * elements converted.
 */
RT_EXPORT extern int tcl_obj_to_fastf_array(Tcl_Interp *interp,
					    Tcl_Obj *list,
					    fastf_t **array,
					    int *array_len);


/**
 * interface to above tcl_obj_to_int_array() routine. This routine
 * expects a character string instead of a Tcl_Obj.
 *
 * Returns the number of elements converted.
 */
RT_EXPORT extern int tcl_list_to_int_array(Tcl_Interp *interp,
					   char *char_list,
					   int **array,
					   int *array_len);


/**
 * interface to above tcl_obj_to_fastf_array() routine. This routine
 * expects a character string instead of a Tcl_Obj.
 *
 * returns the number of elements converted.
 */
RT_EXPORT extern int tcl_list_to_fastf_array(Tcl_Interp *interp,
					     const char *char_list,
					     fastf_t **array,
					     int *array_len);

/** @} */
/* rhc.c */
RT_EXPORT extern int rt_mk_hyperbola(struct rt_pt_node *pts,
				     fastf_t r,
				     fastf_t b,
				     fastf_t c,
				     fastf_t dtol,
				     fastf_t ntol);


/**
 * Initialize a spline matrix for a particular spline type.
 */
RT_EXPORT extern void rt_dspline_matrix(mat_t m, const char *type,
					const double	tension,
					const double	bias);

/**
 * m		spline matrix (see rt_dspline4_matrix)
 * a, b, c, d	knot values
 * alpha:	0..1	interpolation point
 *
 * Evaluate a 1-dimensional spline at a point "alpha" on knot values
 * a, b, c, d.
 */
RT_EXPORT extern double rt_dspline4(mat_t m,
				    double a,
				    double b,
				    double c,
				    double d,
				    double alpha);

/**
 * pt		vector to receive the interpolation result
 * m		spline matrix (see rt_dspline4_matrix)
 * a, b, c, d	knot values
 * alpha:	0..1 interpolation point
 *
 * Evaluate a spline at a point "alpha" between knot pts b & c. The
 * knots and result are all vectors with "depth" values (length).
 *
 */
RT_EXPORT extern void rt_dspline4v(double *pt,
				   const mat_t m,
				   const double *a,
				   const double *b,
				   const double *c,
				   const double *d,
				   const int depth,
				   const double alpha);

/**
 * Interpolate n knot vectors over the range 0..1
 *
 * "knots" is an array of "n" knot vectors.  Each vector consists of
 * "depth" values.  They define an "n" dimensional surface which is
 * evaluated at the single point "alpha".  The evaluated point is
 * returned in "r"
 *
 * Example use:
 *
 * double result[MAX_DEPTH], knots[MAX_DEPTH*MAX_KNOTS];
 * mat_t m;
 * int knot_count, depth;
 *
 * knots = bu_malloc(sizeof(double) * knot_length * knot_count);
 * result = bu_malloc(sizeof(double) * knot_length);
 *
 * rt_dspline4_matrix(m, "Catmull", (double *)NULL, 0.0);
 *
 * for (i=0; i < knot_count; i++)
 *   get a knot(knots, i, knot_length);
 *
 * rt_dspline_n(result, m, knots, knot_count, knot_length, alpha);
 *
 */
RT_EXPORT extern void rt_dspline_n(double *r,
				   const mat_t m,
				   const double *knots,
				   const int n,
				   const int depth,
				   const double alpha);

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


/* db5_attr.c */
/**
 * Define standard attribute types in BRL-CAD geometry.  (See the
 * attributes manual page.)  These should be a collective enumeration
 * starting from 0 and increasing without any gaps in the numbers so
 * db5_standard_attribute() can be used as an index-based iterator.
 */
enum {
    ATTR_REGION = 0,
    ATTR_REGION_ID,
    ATTR_MATERIAL_ID,
    ATTR_AIR,
    ATTR_LOS,
    ATTR_COLOR,
    ATTR_SHADER,
    ATTR_INHERIT,
    ATTR_TIMESTAMP, /* a binary attribute */
    ATTR_NULL
};

/* Enum to characterize status of attributes */
enum {
    ATTR_STANDARD = 0,
    ATTR_USER_DEFINED,
    ATTR_UNKNOWN_ORIGIN
};

struct db5_attr_ctype {
    int attr_type;    /* ID from the main attr enum list */
    int is_binary;   /* false for ASCII attributes; true for binary attributes */
    int attr_subtype; /* ID from attribute status enum list */

    /* names should be specified with alphanumeric characters
     * (lower-case letters, no white space) and will act as unique
     * keys to an object's attribute list */
    const char *name;         /* the "standard" name */
    const char *description;
    const char *examples;
    /* list of alternative names for this attribute: */
    const char *aliases;
    /* property name */
    const char *property;
    /* a longer description for lists outside a table */
    const char *long_description;
};

RT_EXPORT extern const struct db5_attr_ctype db5_attr_std[];

/* Container for holding user-registered attributes */
struct db5_registry{
    void *internal; /**< @brief implementation-specific container for holding information*/
};

/**
 * Initialize a user attribute registry
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_attr_registry_init(struct db5_registry *registry);

/**
 * Free a user attribute registry
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_attr_registry_free(struct db5_registry *registry);

/**
 * Register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_create(struct db5_registry *registry,
	                             int attr_type,
				     int is_binary,
				     int attr_subtype,
				     const char *name,
				     const char *description,
				     const char *examples,
				     const char *aliases,
				     const char *property,
				     const char *long_description);

/**
 * Register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_register(struct db5_registry *registry,
	                               struct db5_attr_ctype *attribute);

/**
 * De-register a user attribute
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_attr_deregister(struct db5_registry *registry,
	                                 const char *name);

/**
 * Look to see if a specific attribute is registered
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern struct db5_attr_ctype *db5_attr_get(struct db5_registry *registry,
	                                             const char *name);

/**
 * Get an array of pointers to all registered attributes
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern struct db5_attr_ctype **db5_attr_dump(struct db5_registry *registry);


/**
 * Function returns the string name for a given standard attribute
 * index.  Index values returned from db5_standardize_attribute()
 * correspond to the names returned from this function, returning the
 * "standard" name.  Callers may also iterate over all names starting
 * with an index of zero until a NULL is returned.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern const char *db5_standard_attribute(int idx);


/**
 * Function returns the string definition for a given standard
 * attribute index.  Index values returned from
 * db5_standardize_attribute_def() correspond to the definition
 * returned from this function, returning the "standard" definition.
 * Callers may also iterate over all names starting with an index of
 * zero until a NULL is returned.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern const char *db5_standard_attribute_def(int idx);

/**
 * Function for recognizing various versions of the DB5 standard
 * attribute names that have been used - returns the attribute type of
 * the supplied attribute name, or -1 if it is not a recognized
 * variation of the standard attributes.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_is_standard_attribute(const char *attrname);

/**
 * Ensures that an attribute set containing standard attributes with
 * non-standard/old/deprecated names gets the standard name added.  It
 * will update the first non-standard name encountered, but will leave
 * any subsequent matching attributes found unmodified if they have
 * different values.  Such "conflict" attributes must be resolved
 * manually.
 *
 * Returns the number of conflicting attributes.
 *
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern size_t db5_standardize_avs(struct bu_attribute_value_set *avs);

/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern int db5_standardize_attribute(const char *attr);
/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_sync_attr_to_comb(struct rt_comb_internal *comb, const struct bu_attribute_value_set *avs, struct directory *dp);
/**
 * PRIVATE: this is new API and should be considered private for the
 * time being.
 */
RT_EXPORT extern void db5_sync_comb_to_attr(struct bu_attribute_value_set *avs, const struct rt_comb_internal *comb);

/* Convenience macros */
#define ATTR_STD(attr) db5_standard_attribute(db5_standardize_attribute(attr))

/* defined in binary_obj.c */
RT_EXPORT extern int rt_mk_binunif(struct rt_wdb *wdbp,
				   const char *obj_name,
				   const char *file_name,
				   unsigned int minor_type,
				   size_t max_count);


/* defined in db5_bin.c */

/**
 * Free the storage associated with a binunif_internal object
 */
RT_EXPORT extern void rt_binunif_free(struct rt_binunif_internal *bip);

/**
 * Diagnostic routine
 */
RT_EXPORT extern void rt_binunif_dump(struct rt_binunif_internal *bip);

/* defined in cline.c */

/**
 * radius of a FASTGEN cline element.
 *
 * shared with rt/do.c
 */
RT_EXPORT extern fastf_t rt_cline_radius;

/* defined in bot.c */
/* TODO - these global variables need to be rolled in to the rt_i structure */
RT_EXPORT extern size_t rt_bot_minpieces;
RT_EXPORT extern size_t rt_bot_tri_per_piece;
RT_EXPORT extern int rt_bot_sort_faces(struct rt_bot_internal *bot,
				       size_t tris_per_piece);
RT_EXPORT extern int rt_bot_decimate(struct rt_bot_internal *bot,
				     fastf_t max_chord_error,
				     fastf_t max_normal_error,
				     fastf_t min_edge_length);

/*
 *  Constants provided and used by the RT library.
 */

/**
 * initial tree start for db tree walkers.
 *
 * Also used by converters in conv/ directory.  Don't forget to
 * initialize ts_dbip before use.
 */
RT_EXPORT extern const struct db_tree_state rt_initial_tree_state;


/** @file librt/vers.c
 *
 * version information about LIBRT
 *
 */

/**
 * report version information about LIBRT
 */
RT_EXPORT extern const char *rt_version(void);


/* WARNING - The function below is *HIGHLY* experimental and will certainly
 * change */
RT_EXPORT void
rt_generate_mesh(int **faces, int *num_faces, point_t **points, int *num_pnts,
	                        struct db_i *dbip, const char *obj, fastf_t delta);


__END_DECLS

#endif /* RAYTRACE_H */
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
