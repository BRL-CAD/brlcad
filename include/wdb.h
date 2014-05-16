/*                           W D B . H
 * BRL-CAD
 *
 * Copyright (c) 1988-2014 United States Government as represented by
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
/** @addtogroup wdb */
/** @{ */
/** @file wdb.h
 *
 *  Interface structures and routines for libwdb
 *
 *  Note -
 *	Rather than using a stdio (FILE *),
 *	we now use a (struct rt_wdb *) parameter.
 *	Rather than calling fopen(), call wdb_fopen();
 *
 */

#ifndef WDB_H
#define WDB_H

#include "common.h"

#include "bu/magic.h"
#include "bu/bitv.h"
#include "bu/list.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"


__BEGIN_DECLS

#ifndef WDB_EXPORT
#  if defined(WDB_DLL_EXPORTS) && defined(WDB_DLL_IMPORTS)
#    error "Only WDB_DLL_EXPORTS or WDB_DLL_IMPORTS can be defined, not both."
#  elif defined(WDB_DLL_EXPORTS)
#    define WDB_EXPORT __declspec(dllexport)
#  elif defined(WDB_DLL_IMPORTS)
#    define WDB_EXPORT __declspec(dllimport)
#  else
#    define WDB_EXPORT
#  endif
#endif


/*
 * In-memory form of database combinations
 */
struct wmember {
    struct bu_list l;
    int wm_op;		/**< @brief  Boolean operation */
    mat_t wm_mat;	/**< @brief  FIXME: Should be a matp_t */
    char *wm_name;
};

#define WMEMBER_INIT_ZERO { BU_LIST_INIT_ZERO, 0, MAT_INIT_IDN, NULL }
#define WMEMBER_INIT(x) { BU_LIST_INIT(&((x)->l)); (x)->wm_op = 0; MAT_IDN((x)->wm_mat); (x)->wm_name = NULL; }
#define WMEMBER_NULL	((struct wmember *)0)
#define WDB_CK_WMEMBER(_p)	BU_CKMAG(_p, WMEMBER_MAGIC, "wmember" );


/**
 *
 * Make a database header (ID) record.
 */
WDB_EXPORT extern int mk_id(struct rt_wdb *fp, const char *title);

/**
 *
 * Make a database header (ID) record, and note the user's preferred
 * editing units (specified as a string).
 *
 * Returns -
 * <0 error
 * 0 success
 */
WDB_EXPORT extern int mk_id_units(struct rt_wdb *fp, const char *title, const char *units);


/**
 *
 * Make a database header (ID) record, and note the user's preferred
 * editing units (specified as a conversion factor).
 *
 * Note that the v4 database format offers only a limited number of
 * choices for the preferred editing units.  If the user is editing in
 * unusual units (like 2.5feet), don't fail to create the database
 * header.
 *
 * In the v5 database, the conversion factor will be stored intact.
 *
 * Note that the database-layer header record will have already been
 * written by db_create().  All we have to do here is update it.
 *
 * Returns -
 * <0 error
 * 0 success
 */
WDB_EXPORT int mk_id_editunits(
    struct rt_wdb *fp,
    const char *title,
    double local2mm );


/*----------------------------------------------------------------------*/


/* libwdb/wdb.c */

/**
 * Library for writing MGED databases from arbitrary procedures.
 * Assumes that some of the structure of such databases are known by
 * the calling routines.
 *
 * It is expected that this library will grow as experience is gained.
 * Routines for writing every permissible solid do not yet exist.
 *
 * Note that routines which are passed point_t or vect_t or mat_t
 * parameters (which are call-by-address) must be VERY careful to
 * leave those parameters unmodified (e.g., by scaling), so that the
 * calling routine is not surprised.
 *
 * Return codes of 0 are OK, -1 signal an error.
 *
 */


/*
 *  Solid conversion routines
 */

/**
 *
 * Make a halfspace.  Specified by distance from origin, and outward
 * pointing normal vector.
 *
 */
WDB_EXPORT extern int mk_half(struct rt_wdb *fp, const char *name, const vect_t norm, fastf_t d);

/**
 *
 * Make a grip pseudo solid.  Specified by a center, normal vector,
 * and magnitude.
 *
 */
WDB_EXPORT int mk_grip(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t center,
    const vect_t normal,
    const fastf_t magnitude);

/**
 *
 * Make a right parallelepiped.  Specified by minXYZ, maxXYZ.
 *
 */
WDB_EXPORT extern int mk_rpp(struct rt_wdb *fp, const char *name, const point_t min, const point_t max);

/**
 *
 * Makes a right angular wedge given a starting vertex located in the,
 * lower left corner, an x and a z direction vector, x, y, and z
 * lengths, and an x length for the top.  The y direction vector is x
 * cross z.
 *
 */
WDB_EXPORT extern int mk_wedge(struct rt_wdb *fp, const char *name, const point_t vert,
			       const vect_t xdirv, const vect_t zdirv,
			       fastf_t xlen, fastf_t ylen, fastf_t zlen,
			       fastf_t x_top_len);

WDB_EXPORT extern int mk_arb4(struct rt_wdb *fp, const char *name, const fastf_t *pts4);

WDB_EXPORT extern int mk_arb5(struct rt_wdb *fp, const char *name, const fastf_t *pts5);

WDB_EXPORT extern int mk_arb6(struct rt_wdb *fp, const char *name, const fastf_t *pts6);

WDB_EXPORT extern int mk_arb7(struct rt_wdb *fp, const char *name, const fastf_t *pts7);

/**
 *
 * All plates with 4 points must be co-planar.  If there are
 * degeneracies (i.e., all 8 vertices are not distinct), then certain
 * requirements must be met.  If we think of the ARB8 as having a top
 * and a bottom plate, the first four points listed must lie on one
 * plate, and the second four points listed must lie on the other
 * plate.
 *
 */
WDB_EXPORT extern int mk_arb8(struct rt_wdb *fp, const char *name, const fastf_t *pts8);

/**
 *
 * Make a sphere with the given center point and radius.
 *
 */
WDB_EXPORT extern int mk_sph(struct rt_wdb *fp, const char *name, const point_t center,
			     fastf_t radius);

/**
 *
 * Make an ellipsoid at the given center point with 3 perp. radius
 * vectors.  The eccentricity of the ellipsoid is controlled by the
 * relative lengths of the three radius vectors.
 *
 */
WDB_EXPORT extern int mk_ell(struct rt_wdb *fp, const char *name, const point_t center,
			     const vect_t a, const vect_t b, const vect_t c);

/**
 *
 * Make a torus.  Specify center, normal, r1: distance from center
 * point to center of solid part, r2: radius of solid part.
 *
 */
WDB_EXPORT extern int mk_tor(struct rt_wdb *fp, const char *name, const point_t center,
			     const vect_t inorm, double r1, double r2);

/**
 *
 * Make a Right Circular Cylinder (special case of the TGC).
 *
 */
WDB_EXPORT extern int mk_rcc(struct rt_wdb *fp, const char *name, const point_t base,
			     const vect_t height, fastf_t radius);

/**
 *
 * Make a Truncated General Cylinder.
 *
 */
WDB_EXPORT extern int mk_tgc(struct rt_wdb *fp, const char *name, const point_t base,
			     const vect_t height, const vect_t a, const vect_t b,
			     const vect_t c, const vect_t d);

/**
 *
 * Makes a right circular cone given the center point of the base
 * circle, a direction vector, a scalar height, and the radii at each
 * end of the cone.
 *
 */
WDB_EXPORT extern int mk_cone(struct rt_wdb *fp, const char *name, const point_t base,
			      const vect_t dirv, fastf_t height, fastf_t rad1,
			      fastf_t rad2);

/**
 *
 * Make a truncated right cylinder, with base and height.  Not just
 * called mk_trc() to avoid conflict with a previous routine of that
 * name with different calling sequence.
 *
 */
WDB_EXPORT extern int mk_trc_h(struct rt_wdb *fp, const char *name, const point_t base,
			       const vect_t height, fastf_t radbase, fastf_t radtop);

/**
 *
 * Convenience wrapper for mk_trc_h().
 *
 */
WDB_EXPORT extern int mk_trc_top(struct rt_wdb *fp, const char *name, const point_t ibase,
				 const point_t itop, fastf_t radbase, fastf_t radtop);

/**
 *
 * Makes a right parabolic cylinder given the origin, or main vertex,
 * a height vector, a breadth vector (B . H must be 0), and a scalar
 * rectangular half-width (for the top of the rpc).
 *
 */
WDB_EXPORT int mk_rpc(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    double half_w );

/**
 *
 * Makes a right hyperbolic cylinder given the origin, or main vertex,
 * a height vector, a breadth vector (B . H must be 0), a scalar
 * rectangular half-width (for the top of the rpc), and the scalar
 * distance from the tip of the hyperbola to the intersection of the
 * asymptotes.
 *
 */
WDB_EXPORT int mk_rhc(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t	half_w,
    fastf_t asymp );

/**
 *
 * Makes an elliptical paraboloid given the origin, a height vector H,
 * a unit vector A along the semi-major axis (A . H must equal 0), and
 * the scalar lengths, r1 and r2, of the semi-major and -minor axes.
 *
 */
WDB_EXPORT int mk_epa(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t r1,
    fastf_t r2 );

/**
 *
 * Makes an elliptical hyperboloid given the origin, a height vector
 * H, a unit vector A along the semi-major axis (A . H must equal 0),
 * the scalar lengths, r1 and r2, of the semi-major and -minor axes,
 * and the distance c between the tip of the hyperboloid and the
 * vertex of the asymptotic cone.
 *
 */
WDB_EXPORT int mk_ehy(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height,
    const vect_t breadth,
    fastf_t r1,
    fastf_t r2,
    fastf_t c );

/**
 *
 * Make a hyperboloid at the given center point with a vertex, height
 * vector, A vector, magnitude of the B vector, and neck to base
 * ratio.
 *
 */
WDB_EXPORT int mk_hyp(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t height_vector,
    const vect_t vectA,
    fastf_t magB,
    fastf_t base_neck_ratio );

/**
 *
 * Makes an elliptical torus given the origin, a plane normal vector
 * N, a vector C along the semi-major axis of the elliptical
 * cross-section, the scalar lengths r and rd, of the radius of
 * revolution and length of semi-minor axis of the elliptical cross
 * section.
 *
 */
WDB_EXPORT int mk_eto(
    struct rt_wdb *wdbp,
    const char *name,
    const point_t vert,
    const vect_t norm,
    const vect_t smajor,
    fastf_t rrot,
    fastf_t sminor );

WDB_EXPORT int mk_metaball(
    struct rt_wdb *wdbp,
    const char *name,
    const size_t nctlpt,	/* number of control points */
    const int method,		/* metaball rendering method */
    const fastf_t threshold,
    const fastf_t *verts[5] );	/* X, Y, Z, fldstr, goo/Beta */

/**
 * Caller is responsible for freeing eqn[]
 *
 * Returns:
 * <0 ERROR
 * 0 OK
 */
WDB_EXPORT extern int mk_arbn(struct rt_wdb *fp, const char *name, size_t neqn, const plane_t *eqn);

WDB_EXPORT extern int mk_ars(struct rt_wdb *fp, const char *name, size_t ncurves, size_t pts_per_curve, fastf_t *curves[]);

/**
 *
 * Given the appropriate parameters, makes the non-geometric
 * constraint object and writes it to the database using
 * wdb_put_internal.  Only supported on database version 5 or above
 *
 */
WDB_EXPORT extern int mk_constraint(struct rt_wdb *wdbp, const char *name, const char *expr);


typedef enum {
    WDB_BINUNIF_FLOAT,
    WDB_BINUNIF_DOUBLE,
    WDB_BINUNIF_CHAR,
    WDB_BINUNIF_UCHAR,
    WDB_BINUNIF_SHORT,
    WDB_BINUNIF_USHORT,
    WDB_BINUNIF_INT,
    WDB_BINUNIF_UINT,
    WDB_BINUNIF_LONG,
    WDB_BINUNIF_ULONG,
    WDB_BINUNIF_LONGLONG,
    WDB_BINUNIF_ULONGLONG,
    WDB_BINUNIF_INT8,
    WDB_BINUNIF_UINT8,
    WDB_BINUNIF_INT16,
    WDB_BINUNIF_UINT16,
    WDB_BINUNIF_INT32,
    WDB_BINUNIF_UINT32,
    WDB_BINUNIF_INT64,
    WDB_BINUNIF_UINT64,
    WDB_BINUNIF_FILE_FLOAT,
    WDB_BINUNIF_FILE_DOUBLE,
    WDB_BINUNIF_FILE_CHAR,
    WDB_BINUNIF_FILE_UCHAR,
    WDB_BINUNIF_FILE_SHORT,
    WDB_BINUNIF_FILE_USHORT,
    WDB_BINUNIF_FILE_INT,
    WDB_BINUNIF_FILE_UINT,
    WDB_BINUNIF_FILE_LONG,
    WDB_BINUNIF_FILE_ULONG,
    WDB_BINUNIF_FILE_LONGLONG,
    WDB_BINUNIF_FILE_ULONGLONG,
    WDB_BINUNIF_FILE_INT8,
    WDB_BINUNIF_FILE_UINT8,
    WDB_BINUNIF_FILE_INT16,
    WDB_BINUNIF_FILE_UINT16,
    WDB_BINUNIF_FILE_INT32,
    WDB_BINUNIF_FILE_UINT32,
    WDB_BINUNIF_FILE_INT64,
    WDB_BINUNIF_FILE_UINT64
} wdb_binunif;


/**
 *
 * Make a uniform binary data object from an array or a data file.
 * Read 'count' values from 'data'.  If 'data_type' is a file, 'count'
 * may be used to only read a subset of a file's contents.  If 'data'
 * is already an in-memory buffer of memory, 'count' values will be
 * copied (which is count * sizeof(data_type) bytes).
 *
 * Files can use a non-positive 'count' to mean "read the whole file",
 * pre-loaded data, however, must provide a positive 'count' otherwise
 * an empty binunif will be created.
 *
 */
WDB_EXPORT extern int mk_binunif(struct rt_wdb *fp, const char *name, const genptr_t data, wdb_binunif data_type, long count);


/*----------------------------------------------------------------------*/


/* libwdb/bot.c */

/**
 *
 * Support for BOT solid (Bag O'Triangles)
 *
 */


WDB_EXPORT int
mk_bot(
    struct rt_wdb *fp,			/**< database file pointer to write to */
    const char *name,			/**< name of bot object to write out */
    unsigned char	mode,		/**< bot mode */
    unsigned char	orientation,	/**< bot orientation */
    unsigned char	error_mode,	/**< may be used to indicate error handling (ignored for now) */
    size_t		num_vertices,	/**< number of vertices */
    size_t		num_faces,	/**< number of faces */
    fastf_t		*vertices,	/**< array of floats for vertices [num_vertices*3] */
    int			*faces,		/**< array of ints for faces [num_faces*3] */
    fastf_t		*thickness,	/**< array of plate mode
					 * thicknesses (corresponds to
					 * array of faces) NULL for
					 * modes RT_BOT_SURFACE and
					 * RT_BOT_SOLID.
					 */
    struct bu_bitv	*face_mode 	/**< a flag for each face
					 * indicating thickness is
					 * appended to hit point,
					 * otherwise thickness is
					 * centered about hit point
					 */
    );

WDB_EXPORT int
mk_bot_w_normals(
    struct rt_wdb *fp,			/**< database file pointer to write to */
    const char *name,			/**< name of bot object to write out */
    unsigned char	mode,		/**< bot mode */
    unsigned char	orientation,	/**< bot orientation */
    unsigned char	flags,		/**< additional bot flags */
    size_t		num_vertices,	/**< number of bot vertices */
    size_t		num_faces,	/**< number of bot faces */
    const fastf_t		*vertices,	/**< array of floats for vertices [num_vertices*3] */
    const int			*faces,		/**< array of ints for faces [num_faces*3] */
    const fastf_t		*thickness,	/**< array of plate mode
					 * thicknesses (corresponds to
					 * array of faces) NULL for
					 * modes RT_BOT_SURFACE and
					 * RT_BOT_SOLID.
					 */
    struct bu_bitv	*face_mode,	/**< a flag for each face
					 * indicating thickness is
					 * appended to hit point,
					 * otherwise thickness is
					 * centered about hit point
					 */
    size_t		num_normals,	/**< number of unit normals in normals array */
    fastf_t		*normals,	/**< array of floats for normals [num_normals*3] */
    int			*face_normals	/**< array of ints (indices
					 * into normals array), must
					 * have 3*num_faces entries
					 */
    );


/*----------------------------------------------------------------------*/


/* libwdb/brep.cpp */

/**
 *
 * Library for writing BREP objects into
 * MGED databases from arbitrary procedures.
 *
 */


/**
 *
 * Create a brep in the geometry file.
 *
 */
WDB_EXPORT int mk_brep( struct rt_wdb* wdbp, const char* name, ON_Brep* brep );


/*----------------------------------------------------------------------*/


/* libwdb/nurb.c */

/**
 *
 * Library for writing NURB objects into
 * MGED databases from arbitrary procedures.
 *
 */


/**
 *
 * Output an array of B-spline (NURBS) surfaces which comprise a
 * solid.  The surface is freed when it is written.
 *
 */
WDB_EXPORT int mk_bspline( struct rt_wdb *wdbp, const char *name, struct face_g_snurb **surfs );


/*----------------------------------------------------------------------*/


/* libwdb/nmg.c */

/**
 *
 * libwdb support for writing an NMG to disk.
 *
 */


/**
 *
 * The NMG is freed after being written.
 *
 * Returns -
 * <0 error
 * 0 OK
 *
 */
WDB_EXPORT int mk_nmg( struct rt_wdb *filep, const char *name, struct model *m );

/**
 *
 * For ray-tracing speed, many database conversion routines like to
 * offer the option of converting NMG objects to bags of triangles
 * (BoT).  Here is a convenience routine to replace the old routine
 * write_shell_as_polysolid.  (obsolete since BRL-CAD 6.0)
 *
 */
WDB_EXPORT int mk_bot_from_nmg( struct rt_wdb *ofp, const char *name, struct shell *s );


/*----------------------------------------------------------------------*/


/* libwdb/sketch.c */

/**
 *
 * Support for sketches
 *
 */


WDB_EXPORT int mk_sketch(
    struct rt_wdb *fp,
    const char *name,
    const struct rt_sketch_internal *skt );


/*----------------------------------------------------------------------*/


/* libwdb/extrude.c */

/**
 *
 * Support for extrusion solids
 *
 */


WDB_EXPORT int mk_extrusion(
    struct rt_wdb *fp,
    const char *name,
    const char *sketch_name,
    const point_t V,
    const vect_t h,
    const vect_t u_vec,
    const vect_t v_vec,
    int keypoint );


/*----------------------------------------------------------------------*/


/* libwdb/cline.c */

/**
 *
 * Support for cline solids (kludges from FASTGEN)
 *
 */


WDB_EXPORT int mk_cline(
    struct rt_wdb *fp,
    const char *name,
    const point_t V,
    const vect_t height,
    fastf_t radius,
    fastf_t thickness );


/*----------------------------------------------------------------------*/


/* libwdb/pipe.c */

/**
 *
 * Support for particles and pipes.  Library for writing geometry
 * databases from arbitrary procedures.
 *
 * Note that routines which are passed point_t or vect_t or mat_t
 * parameters (which are call-by-address) must be VERY careful to
 * leave those parameters unmodified (e.g., by scaling), so that the
 * calling routine is not surprised.
 *
 * Return codes of 0 are OK, -1 signal an error.
 *
 */


/**
 *
 * Returns -
 * 0 OK
 * <0 failure
 *
 */
WDB_EXPORT extern int mk_particle(struct rt_wdb *fp, const char *name, point_t vertex,
				  vect_t height, double vradius, double hradius);

/**
 *
 * Note that the linked list of pipe segments headed by 'headp' must
 * be freed by the caller.  mk_pipe_free() can be used.
 *
 * Returns -
 * 0 OK
 * <0 failure
 *
 */
WDB_EXPORT extern int mk_pipe(struct rt_wdb *fp, const char *name, struct bu_list *headp);

/**
 *
 * Release the storage from a list of pipe segments.  The head is left
 * in initialized state (i.e., forward & back point to head).
 */
WDB_EXPORT void mk_pipe_free( struct bu_list *headp );

/**
 *
 * Add another pipe segment to the linked list of pipe segments.
 *
 */
WDB_EXPORT void mk_add_pipe_pt(
    struct bu_list *headp,
    const point_t coord,
    double od,
    double id,
    double bendradius );

/**
 *
 * initialize a linked list of pipe segments with the first segment
 *
 */
WDB_EXPORT void mk_pipe_init( struct bu_list *headp );


/* strsol primitives */

WDB_EXPORT extern int mk_dsp(struct rt_wdb *fp, const char *name, const char *file,
			     size_t xdim, size_t ydim, const matp_t mat);

WDB_EXPORT extern int mk_ebm(struct rt_wdb *fp, const char *name, const char *file,
				   size_t xdim, size_t ydim, fastf_t tallness, const matp_t mat);

WDB_EXPORT extern int mk_hrt(struct rt_wdb *fp, const char *name, const point_t center,
			     const vect_t x, const vect_t y, const vect_t z, const fastf_t dist);

WDB_EXPORT extern int mk_vol(struct rt_wdb *fp, const char *name, const char *file,
			     size_t xdim, size_t ydim, size_t zdim, size_t lo, size_t hi,
			     const vect_t cellsize, const matp_t mat);


/**
 *
 * Create a submodel solid.  If file is NULL or "", the treetop refers
 * to the current database.  Treetop is the name of a single database
 * object in 'file'.  meth is 0 (RT_PART_NUBSPT) or 1
 * (RT_PART_NUGRID).  method 0 is what is normally used.
 */
WDB_EXPORT extern int mk_submodel(struct rt_wdb *fp, const char *name, const char *file,
				  const char *treetop, int meth);


/*----------------------------------------------------------------------*/


/* libwdb/mater.c */

/**
 *
 * Interface for writing region-id-based color tables to the database.
 *
 */


/**
 *
 * Given that the color table has been built up by successive calls to
 * rt_color_addrec(), write it into the database.
 *
 */
WDB_EXPORT int mk_write_color_table( struct rt_wdb *ofp );

/*
 *  Combination (region&group) construction routines
 *
 *  First you build a list of nodes with mk_addmember,
 *  then you output the combination.
 */


/**
 *
 * Obtain dynamic storage for a new wmember structure, fill in the
 * name, default the operation and matrix, and add to doubly linked
 * list.  In typical use, a one-line call is sufficient.  To change
 * the defaults, catch the pointer that is returned, and adjust the
 * structure to taste.
 *
 * The caller is responsible for initializing the header structures
 * forward and backward links.
 */
WDB_EXPORT extern struct wmember *mk_addmember(const char	*name,
					       struct bu_list	*headp,
					       mat_t		mat,
					       int		op);

#define mk_lcomb(_fp, _name, _headp, _rf, _shadername, _shaderargs, _rgb, _inh)	\
	mk_comb(_fp, _name, &((_headp)->l), _rf, _shadername, _shaderargs, \
		_rgb, 0, 0, 0, 0, _inh, 0, 0)

/* mk_lrcomb() would not append, and did not have GIFT semantics */
/* mk_lrcomb() had (struct wmember *) head, need (struct bu_list *) */
#define mk_lrcomb(fp, name, _headp, region_flag, shadername, shaderargs, rgb, id, air, material, los, inherit_flag )	\
	mk_comb( fp, name, &((_headp)->l), region_flag, shadername, shaderargs, \
		rgb, id, air, material, los, inherit_flag, 0, 0 )


/**
 * Make a combination, where the members are described by a linked
 * list of wmember structs.
 *
 * The linked list is freed when it has been output.
 *
 * Has many operating modes.
 *
 * Returns -
 * -1 ERROR
 * 0 OK
 */
WDB_EXPORT int mk_comb(
    struct rt_wdb	*wdbp,			/**< database to write to */
    const char		*combname,		/**< name of the combination */
    struct bu_list	*headp,			/**< Made by mk_addmember() */
    int			region_kind,		/**< 1 => region.  'P' and 'V' for FASTGEN */
    const char		*shadername,		/**< shader name, or NULL */
    const char		*shaderargs,		/**< shader args, or NULL */
    const unsigned char	*rgb,			/**< NULL => no color */
    int			id,			/**< region_id */
    int			air,			/**< aircode */
    int			material,		/**< GIFTmater */
    int			los,			/**< line-of-sight thickness equivalence */
    int			inherit,		/**< whether objects below inherit from this comb */
    int			append_ok,		/**< 0 = obj must not exit */
    int			gift_semantics);	/**<  0 = pure, 1 = gift */

/** Convenience routines for quickly making combinations */

/**
 *
 * Convenience interface to make a combination with a single member.
 */
WDB_EXPORT int mk_comb1( struct rt_wdb *fp,
			 const char *combname,
			 const char *membname,
			 int regflag );


/**
 *
 * Convenience routine to make a region with shader and rgb possibly
 * set.
 */
WDB_EXPORT int mk_region1(
    struct rt_wdb *fp,
    const char *combname,
    const char *membname,
    const char *shadername,
    const char *shaderargs,
    const unsigned char *rgb );

/* Values for wm_op.  These must track db.h */
#define WMOP_INTERSECT	'+'
#define WMOP_SUBTRACT	'-'
#define WMOP_UNION	'u'

/* Convenient definitions */
#define mk_lfcomb(fp, name, headp, region)		mk_lcomb( fp, name, headp, \
	region, (char *)0, (char *)0, (unsigned char *)0, 0 );

/*
 *  Routines to establish conversion factors
 */

 /**
 *
 * Given a string conversion value, find the appropriate factor, and
 * establish it.
 *
 * Returns -
 * -1 error
 * 0 OK
 */
WDB_EXPORT extern int mk_conversion(char *units_string);


/**
 *
 * Establish a new conversion factor for LIBWDB routines.
 *
 * Returns -
 * -1 error
 * 0 OK
 */
WDB_EXPORT extern int mk_set_conversion(double val);

/**
 * This internal variable should not be directly modified;
 * call mk_conversion() or mk_set_conversion() instead.
 */
WDB_EXPORT extern double mk_conv2mm;		/**< @brief Conversion factor to mm */

/**
 *  Set this variable to either 4 or 5, depending on which version of
 *  the database you wish to write.
 */
WDB_EXPORT extern int mk_version;		/**< @brief  Which version database to write */

/*
 *  Internal routines
 */

WDB_EXPORT void mk_freemembers( struct bu_list *headp );

#define mk_export_fwrite(wdbp, name, gp, id)	wdb_export(wdbp, name, gp, id, mk_conv2mm)

/*
 *	Dynamic geometry routines
 */

/**
 * This routine is intended to be used to make a hole in some
 * geometry.  The hole is described using the same parameters as an
 * RCC, and the hole is represented as an RCC. The objects to be
 * "holed" are passed in as a list of "struct directory" pointers. The
 * objects pointed at by this list must be combinations. The "struct
 * rt_wdb" pointer passed in indicates what model this hole should
 * appear in.
 *
 * The end state after this routine runs is a modified model with a
 * new RCC primitive having a name of the form "make_hole_X" (where X
 * is some integer). The combinations specified in the list will be
 * modified as follows:
 *
 *        before                          after
 *          |                              /\
 *          u                             u  -
 *   orig_comb_tree          orig_comb_tree  make_hole_X
 *
 * The modified combination is written to the struct rt_wdb. Note that
 * to do dynamic geometry a "wdb_dbopen" would normally be called on
 * an already existing (and possibly prepped) model.  Using the
 * RT_WDB_TYPE_DB_INMEM parameter in this call will result in geometry
 * changes that only exist in memory and will not be permanently
 * stored in the original database.
 *
 * This routine should be preceded by a call to "rt_unprep" and
 * followed by a call to "rt_reprep".
 */

WDB_EXPORT extern int make_hole(struct rt_wdb *wdbp,
				point_t hole_start,
				vect_t hole_depth,
				fastf_t hole_radius,
				int num_objs,
				struct directory **dp);


/**
 * This routine provides a quick approach to simply adding a hole to
 * existing prepped geometry.  The geometry must already be prepped
 * prior to calling this routine. After calling this routine, the
 * geometry is ready for raytracing (no other routine need to be
 * called).
 *
 * A new RCC primitive is created and written to the database
 * (wdbp). Note that this will be temporary if the wdbp pointer was
 * created by a call to wdb_dbopen with the RT_WDB_TYPE_DB_INMEM flag.
 *
 * The "regions" parameter is a list of "struct region" pointers
 * (prepped regions) to get holed.  The regions structures are
 * modified, but the on disk region records are never modified, so the
 * actual holes will never be permanent regardless of how "wdbp" was
 * opened.
 *
 * There is no need to call "rt_unprep" nor "rt_reprep" with this
 * routine.
 */
WDB_EXPORT extern int make_hole_in_prepped_regions(struct rt_wdb *wdbp,
						   struct rt_i *rtip,
						   point_t hole_start,
						   vect_t hole_depth,
						   fastf_t radius,
						   struct bu_ptbl *regions);


__END_DECLS

#endif /* WDB_H */

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
