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

#ifndef BN_VLIST_H
#define BN_VLIST_H

#include "common.h"
#include <stdio.h> /* for FILE */
#include "vmath.h"
#include "bu/magic.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bn/defines.h"

__BEGIN_DECLS

/*----------------------------------------------------------------------*/
/** @addtogroup bn_vlist
 *
 * @brief
 * Definitions for handling lists of vectors (really vertices, or
 * points) and polygons in 3-space.  Intended for common handling of
 * wireframe display information, in the full resolution that is
 * calculated in.
 *
 * On 32-bit machines, BN_VLIST_CHUNK of 35 results in bn_vlist
 * structures just less than 1k bytes.
 *
 * The head of the doubly linked list can be just a "struct bu_list"
 * head.
 *
 * To visit all the elements in the vlist:
 *	for (BU_LIST_FOR(vp, bn_vlist, hp)) {
 *		int	i;
 *		int	nused = vp->nused;
 *		int	*cmd = vp->cmd;
 *		point_t *pt = vp->pt;
 *		for (i = 0; i < nused; i++, cmd++, pt++) {
 *			access(*cmd, *pt);
 *			access(vp->cmd[i], vp->pt[i]);
 *		}
 *	}
 */
/** @{ */
/** @file bn/vlist.h */


#define BN_VLIST_CHUNK 35		/**< @brief 32-bit mach => just less than 1k */

struct bn_vlist  {
    struct bu_list l;		/**< @brief magic, forw, back */
    size_t nused;		/**< @brief elements 0..nused active */
    int cmd[BN_VLIST_CHUNK];	/**< @brief VL_CMD_* */
    point_t pt[BN_VLIST_CHUNK];	/**< @brief associated 3-point/vect */
};
#define BN_VLIST_NULL	((struct bn_vlist *)0)
#define BN_CK_VLIST(_p) BU_CKMAG((_p), BN_VLIST_MAGIC, "bn_vlist")

/* should these be enum? -Erik */
/* Values for cmd[] */
#define BN_VLIST_LINE_MOVE		0	/**< @brief specify new line */
#define BN_VLIST_LINE_DRAW		1	/**< @brief subsequent line vertex */
#define BN_VLIST_POLY_START		2	/**< @brief pt[] has surface normal */
#define BN_VLIST_POLY_MOVE		3	/**< @brief move to first poly vertex */
#define BN_VLIST_POLY_DRAW		4	/**< @brief subsequent poly vertex */
#define BN_VLIST_POLY_END		5	/**< @brief last vert (repeats 1st), draw poly */
#define BN_VLIST_POLY_VERTNORM		6	/**< @brief per-vertex normal, for interpolation */
#define BN_VLIST_TRI_START		7	/**< @brief pt[] has surface normal */
#define BN_VLIST_TRI_MOVE		8	/**< @brief move to first triangle vertex */
#define BN_VLIST_TRI_DRAW		9	/**< @brief subsequent triangle vertex */
#define BN_VLIST_TRI_END		10	/**< @brief last vert (repeats 1st), draw poly */
#define BN_VLIST_TRI_VERTNORM		11	/**< @brief per-vertex normal, for interpolation */
#define BN_VLIST_POINT_DRAW		12	/**< @brief Draw a single point */
#define BN_VLIST_POINT_SIZE		13	/**< @brief specify point pixel size */
#define BN_VLIST_LINE_WIDTH		14	/**< @brief specify line pixel width */
#define BN_VLIST_DISPLAY_MAT		15	/**< @brief specify the model matrix */
#define BN_VLIST_MODEL_MAT		16	/**< @brief specify the display matrix */
#define BN_VLIST_CMD_MAX		16	/**< @brief Max command number */
/**
 * Applications that are going to use BN_ADD_VLIST and BN_GET_VLIST
 * are required to execute this macro once, on their _free_hd:
 * BU_LIST_INIT(&_free_hd);
 *
 * Note that BN_GET_VLIST and BN_FREE_VLIST are non-PARALLEL.
 */
#define BN_GET_VLIST(_free_hd, p) do {\
	(p) = BU_LIST_FIRST(bn_vlist, (_free_hd)); \
	if (BU_LIST_IS_HEAD((p), (_free_hd))) { \
	    BU_ALLOC((p), struct bn_vlist); \
	    (p)->l.magic = BN_VLIST_MAGIC; \
	} else { \
	    BU_LIST_DEQUEUE(&((p)->l)); \
	} \
	(p)->nused = 0; \
    } while (0)

/** Place an entire chain of bn_vlist structs on the freelist _free_hd */
#define BN_FREE_VLIST(_free_hd, hd) do { \
	BU_CK_LIST_HEAD((hd)); \
	BU_LIST_APPEND_LIST((_free_hd), (hd)); \
    } while (0)

#define BN_ADD_VLIST(_free_hd, _dest_hd, pnt, draw) do { \
	struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bn_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BN_VLIST_CHUNK) { \
	    BN_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	VMOVE(_vp->pt[_vp->nused], (pnt)); \
	_vp->cmd[_vp->nused++] = (draw); \
    } while (0)

/** Change the transformation matrix to display */
#define BN_VLIST_SET_DISP_MAT(_free_hd, _dest_hd, _ref_pt) do { \
	struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bn_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BN_VLIST_CHUNK) { \
	    BN_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	VMOVE(_vp->pt[_vp->nused], (_ref_pt)); \
	_vp->cmd[_vp->nused++] = BN_VLIST_DISPLAY_MAT; \
    } while (0)

/** Change the transformation matrix to model */
#define BN_VLIST_SET_MODEL_MAT(_free_hd, _dest_hd) do { \
	struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bn_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BN_VLIST_CHUNK) { \
	    BN_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	_vp->cmd[_vp->nused++] = BN_VLIST_MODEL_MAT; \
    } while (0)

/** Set a point size to apply to the vlist elements that follow. */
#define BN_VLIST_SET_POINT_SIZE(_free_hd, _dest_hd, _size) do { \
	struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bn_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BN_VLIST_CHUNK) { \
	    BN_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	_vp->pt[_vp->nused][0] = (_size); \
	_vp->cmd[_vp->nused++] = BN_VLIST_POINT_SIZE; \
    } while (0)

/** Set a line width to apply to the vlist elements that follow. */
#define BN_VLIST_SET_LINE_WIDTH(_free_hd, _dest_hd, _width) do { \
	struct bn_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bn_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BN_VLIST_CHUNK) { \
	    BN_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	_vp->pt[_vp->nused][0] = (_width); \
	_vp->cmd[_vp->nused++] = BN_VLIST_LINE_WIDTH; \
    } while (0)


BN_EXPORT extern size_t bn_vlist_cmd_cnt(struct bn_vlist *vlist);
BN_EXPORT extern int bn_vlist_bbox(struct bu_list *vlistp, point_t *bmin, point_t *bmax, size_t *length);



/**
 * For plotting, a way of separating plots into separate color vlists:
 * blocks of vlists, each with an associated color.
 */
struct bn_vlblock {
    uint32_t magic;
    size_t nused;
    size_t max;
    long *rgb;		/**< @brief rgb[max] variable size array */
    struct bu_list *head;		/**< @brief head[max] variable size array */
    struct bu_list *free_vlist_hd;	/**< @brief where to get/put free vlists */
};
#define BN_CK_VLBLOCK(_p)	BU_CKMAG((_p), BN_VLBLOCK_MAGIC, "bn_vlblock")


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
BN_EXPORT extern void bn_vlist_3string(struct bu_list *vhead,
				       struct bu_list *free_hd,
				       const char *string,
				       const point_t origin,
				       const mat_t rot,
				       double scale);

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
BN_EXPORT extern void bn_vlist_2string(struct bu_list *vhead,
				       struct bu_list *free_hd,
				       const char *string,
				       double x,
				       double y,
				       double scale,
				       double theta);

/**
 * Returns the description of a vlist cmd.  Caller is responsible
 * for freeing the returned string.
 */
BN_EXPORT extern const char *bn_vlist_get_cmd_description(int cmd);

/**
 * Validate an bn_vlist chain for having reasonable values inside.
 * Calls bu_bomb() if not.
 *
 * Returns -
 * npts Number of point/command sets in use.
 */
BN_EXPORT extern size_t bn_ck_vlist(const struct bu_list *vhead);


/**
 * Duplicate the contents of a vlist.  Note that the copy may be more
 * densely packed than the source.
 */
BN_EXPORT extern void bn_vlist_copy(struct bu_list *vlists,
				    struct bu_list *dest,
				    const struct bu_list *src);


/**
 * Convert a vlist chain into a blob of network-independent binary,
 * for transmission to another machine.
 *
 * The result is stored in a vls string, so that both the address and
 * length are available conveniently.
 */
BN_EXPORT extern void bn_vlist_export(struct bu_vls *vls,
				      struct bu_list *hp,
				      const char *name);


/**
 * Convert a blob of network-independent binary prepared by
 * vls_vlist() and received from another machine, into a vlist chain.
 */
BN_EXPORT extern void bn_vlist_import(struct bu_list *vlists,
				      struct bu_list *hp,
				      struct bu_vls *namevls,
				      const unsigned char *buf);

BN_EXPORT extern void bn_vlist_cleanup(struct bu_list *hd);

BN_EXPORT extern struct bn_vlblock *bn_vlblock_init(struct bu_list *free_vlist_hd, /* where to get/put free vlists */
						    int max_ent);

BN_EXPORT extern void bn_vlblock_free(struct bn_vlblock *vbp);

BN_EXPORT extern struct bu_list *bn_vlblock_find(struct bn_vlblock *vbp,
						 int r,
						 int g,
						 int b);


BN_EXPORT void bn_vlist_rpp(struct bu_list *vlists, struct bu_list *hd, const point_t minn, const point_t maxx);

/**
 * Output a bn_vlblock object in extended UNIX-plot format, including
 * color.
 */
BN_EXPORT extern void bn_plot_vlblock(FILE *fp,
				      const struct bn_vlblock *vbp);

/**
 * Output a vlist as an extended 3-D floating point UNIX-Plot file.
 * You provide the file.  Uses libplot3 routines to create the
 * UNIX-Plot output.
 */
BN_EXPORT extern void bn_vlist_to_uplot(FILE *fp,
					const struct bu_list *vhead);

/** @} */

__END_DECLS

#endif  /* BN_VLIST_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
