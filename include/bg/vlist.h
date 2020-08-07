/*                        V L I S T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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

#ifndef BG_VLIST_H
#define BG_VLIST_H

#include "common.h"
#include <stdio.h> /* for FILE */
#include "vmath.h"
#include "bu/magic.h"
#include "bu/vls.h"
#include "bg/defines.h"

__BEGIN_DECLS

/*----------------------------------------------------------------------*/
/** @addtogroup bg_vlist
 *
 * @brief
 * Definitions for handling lists of vectors (really vertices, or
 * points) and polygons in 3-space.  Intended for common handling of
 * wireframe display information, in the full resolution that is
 * calculated in.
 */
/** @{ */
/** @file bg/vlist.h */

/* Opaque container that stores unused vlists for re-use */
struct bg_vlist_queue_impl;
struct bg_vlist_queue {
    struct bg_vlist_queue_impl *i;
};

/* Establish a queue to hold free vlist entries - generally, this is the first
 * object to create when working with vlists. */
BG_EXPORT extern struct bg_vlist_queue *
bg_vlist_queue_create();

/* Remove a vlist queue. Will invalidate all queue-related vlist operations
 * using this queue - generally, this is the last object to free.  */
BG_EXPORT extern void
bg_vlist_queue_destroy(struct bg_vlist_queue *);


/* Opaque container that stores bg_vlists.  Used internally - not manipulated
 * by calling applications. The caller may associate a bg_vlist_queue with one
 * or more vlists. If there is a queue associated:
 *
 * 1. Point/cmd additions will first use memory from the queue - only if
 *    none are available will new entities be allocated.
 * 2. A bg_vlist_destroy call will place the internal memory in the queue,
 *    rather than freeing it.
 *
 * TODO: multithreaded operations using bg_vlists with a queue are not
 * currently supported.  Should be able to make that work transparently
 * from the caller's perspective...
 */
struct bg_vlist_impl;
struct bg_vlist {
    struct bg_vlist_impl *i;
    struct bg_vlist_queue *q;
};

/* q is where to get/put free vlist entries - if NULL, all memory is managed
 * locally in this specific vlist */
BG_EXPORT extern struct bg_vlist *
bg_vlist_create(struct bg_vlist_queue *q);

/* destroy vlist - do this for all vlists using a queue before destroying the
 * queue */
BG_EXPORT extern void
bg_vlist_destroy(struct bg_vlist *l);


/* Values for cmd[] */
typedef enum {
    /* Points */
    BG_VLIST_POINT_DRAW,	/**< @brief Draw a single point */
    BG_VLIST_POINT_SIZE,	/**< @brief specify point pixel size */
    /* Wireframes */
    BG_VLIST_LINE_MOVE,		/**< @brief specify new line */
    BG_VLIST_LINE_DRAW,		/**< @brief subsequent line vertex */
    BG_VLIST_LINE_WIDTH,	/**< @brief specify line pixel width */
    /* Triangles */
    BG_VLIST_TRI_START,		/**< @brief pt[] has surface normal */
    BG_VLIST_TRI_MOVE,		/**< @brief move to first triangle vertex */
    BG_VLIST_TRI_DRAW,		/**< @brief subsequent triangle vertex */
    BG_VLIST_TRI_END,		/**< @brief last vert (repeats 1st), draw poly */
    BG_VLIST_TRI_VERTNORM,	/**< @brief per-vertex normal, for interpolation */
    /* Polygons */
    BG_VLIST_POLY_START,	/**< @brief pt[] has surface normal */
    BG_VLIST_POLY_MOVE,		/**< @brief move to first poly vertex */
    BG_VLIST_POLY_DRAW,		/**< @brief subsequent poly vertex */
    BG_VLIST_POLY_END,		/**< @brief last vert (repeats 1st), draw poly */
    BG_VLIST_POLY_VERTNORM,	/**< @brief per-vertex normal, for interpolation */
    /* Display Modes */
    BG_VLIST_DISPLAY_MAT,	/**< @brief specify the model matrix */
    BG_VLIST_MODEL_MAT,		/**< @brief specify the display matrix */
    BG_VLIST_NULL 		/**< @brief no-op command */
} bg_vlist_cmd_t;

/* The vlist specification is serial and state based - if a matrix or point
 * size is set via command, it applies to the points/segments defined after that
 * command is introduced. */

BG_EXPORT extern long
bg_vlist_npts(struct bg_vlist *v);

BG_EXPORT extern long
bg_vlist_ncmds(struct bg_vlist *v);

BG_EXPORT extern long
bg_vlist_add(struct bg_vlist *v, long i, bg_vlist_cmd_t cmd, point_t *p);

BG_EXPORT extern bg_vlist_cmd_t
bg_vlist_get(point_t *op, struct bg_vlist *v, long i, point_t *p, bg_vlist_cmd_t cmd);

BG_EXPORT extern long
bg_vlist_rm(struct bg_vlist *v, bg_vlist_cmd_t cmd, point_t *p, long i);

BG_EXPORT extern long
bg_vlist_get_pnt_size(struct bg_vlist *v, size_t i, point_t *p);

/* Analytic functions */

/* Returns the index of the closest point in the vlist to the test point.  If
 * cp is not NULL, also calculates and returns the closest point on the vlist
 * polylines. */
BG_EXPORT extern long
bg_vlist_closest_pnt(point_t *cp, struct bg_vlist *v, point_t *tp);

/* Calculate and return the bounding box and polyline length of the vlist */
BG_EXPORT extern int
bg_vlist_bbox(point_t *bmin, point_t *bmax, int *poly_length, struct bg_vlist *v);

/* vlblocks associate colors with vlists */
struct bg_vlblock_impl;
struct bg_vlblock {
    struct bg_vlblock_impl *i;
};

/* Create an empty bg_vlblock */
BG_EXPORT extern struct bg_vlblock *
bg_vlblock_create();

/* Destroy a bg_vlblock.  Does not destroy vlists in the block or
 * their associated queues - to fully clean up bg_vlblock:
 *
 * 1. Find and destroy all vlists in the block.
 * 2. Destroy the bg_vlblock
 * 3. Destroy the bg_vlist_queue(s) in use by the app.
 */
BG_EXPORT extern void
bg_vlblock_destroy(struct bg_vlblock *b);

BG_EXPORT extern int
bg_vlist_set_color(struct bg_vlblock *vb, struct bg_vlist *v, struct bu_color *c);

BG_EXPORT extern struct bu_color *
bg_vlist_get_color(struct bg_vlblock *vb, struct bg_vlist *v);

BG_EXPORT extern int
bg_vlblock_list_colors(struct bu_color **c, struct bg_vlblock *vb);

BG_EXPORT extern int
bg_vlblock_find(struct bg_vlist **, struct bg_vlblock *vb, struct bu_color *c);


/**
 * Convert a string to a vlist.
 *
 * 'scale' is the width, in mm, of one character.
 *
 * @param vhead   vhead
 * @param free_hd source of free vlists
 * @param string  string of chars to be plotted
 * @param origin  lower left corner of 1st char
 * @param rot	  Transform matrix (WARNING: may xlate)
 * @param scale   scale factor to change 1x1 char sz
 *
 */
BG_EXPORT extern struct bg_vlist *
bg_vlist_3string(
	const char *string,
	const point_t origin,
	const mat_t rot,
	double scale,
       	struct bg_vlist_queue *q
	);

/**
 * Convert string to vlist in 2D
 *
 * A simpler interface, for those cases where the text lies in the X-Y
 * plane.
 *
 * @param vhead		vhead
 * @param free_hd	source of free vlists
 * @param string	string of chars to be plotted
 * @param x		lower left corner of 1st char
 * @param y		lower left corner of 1st char
 * @param scale		scale factor to change 1x1 char sz
 * @param theta 	degrees ccw from X-axis
 *
 */
BG_EXPORT extern struct bg_vlist *
bg_vlist_2string(
	const char *string,
	double x,
	double y,
	double scale,
	double theta,
       	struct bg_vlist_queue *q
	);

/**
 * Returns string description of a vlist cmd.
 */
BG_EXPORT extern const char *
bg_vlist_cmd_str(bg_vlist_cmd_t cmd);

/**
 * Validate an bg_vlist chain for having reasonable values inside.
 *
 * Returns -
 * Number of point/command sets in use or -1 if invalid.
 */
BG_EXPORT extern long
bg_vlist_valid(const struct bu_list *vhead);


/**
 * Duplicate the contents of a vlist.  Note that the copy may be more
 * densely packed than the source.
 */
BG_EXPORT extern void
bg_vlist_copy(struct bg_vlist *dest, const struct bg_vlist *src);


/**
 * Convert a vlist chain into a blob of network-independent binary,
 * for transmission to another machine.
 *
 * The result is stored in a vls string, so that both the address and
 * length are available conveniently.
 */
BG_EXPORT extern void
bg_vlist_export(
	struct bu_vls *vls,
	struct bg_vlist *v,
	const char *name
	);


/**
 * Convert a blob of network-independent binary prepared by
 * vls_vlist() and received from another machine, into a vlist chain.
 */
BG_EXPORT extern void
bg_vlist_import(
	struct bg_vlist *vout,
	struct bg_vlist_queue *q,
	struct bu_vls *input
	);


/**
 * Output a bg_vlblock object in extended UNIX-plot format, including
 * color.
 */
BG_EXPORT extern void
bg_vlblock_plot3(
	FILE *fp,
	const struct bg_vlblock *vbp
	);

/**
 * Output a vlist as an extended 3-D floating point UNIX-Plot file.
 * You provide the file.  Uses libplot3 routines to create the
 * UNIX-Plot output.
 */
BG_EXPORT extern void
bg_vlist_plot3(
	FILE *fp,
	const struct bu_list *vhead
	);

/** @} */

__END_DECLS

#endif  /* BG_VLIST_H */
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
