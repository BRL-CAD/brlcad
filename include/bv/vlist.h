/*                        V L I S T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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
/** @addtogroup bv_vlist
 *
 * @brief
 * Definitions for handling lists of vectors (really vertices, or
 * points) and polygons in 3-space.  Intended for common handling of
 * wireframe display information, in the full resolution that is
 * calculated in.
 *
 * On 32-bit machines, BV_VLIST_CHUNK of 35 results in bv_vlist
 * structures just less than 1k bytes.
 *
 * The head of the doubly linked list can be just a "struct bu_list"
 * head.
 *
 * To visit all the elements in the vlist:
 *	for (BU_LIST_FOR(vp, bv_vlist, hp)) {
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
/** @file bv/vlist.h */

#ifndef BV_VLIST_H
#define BV_VLIST_H

#include "common.h"
#include <stdio.h> /* for FILE */
#include "vmath.h"
#include "bu/magic.h"
#include "bu/list.h"
#include "bu/vls.h"
#include "bv/defines.h"

__BEGIN_DECLS

#define BV_VLIST_CHUNK 35		/**< @brief 32-bit mach => just less than 1k */

struct bv_vlist  {
    struct bu_list l;		/**< @brief magic, forw, back */
    size_t nused;		/**< @brief elements 0..nused active */
    int cmd[BV_VLIST_CHUNK];	/**< @brief VL_CMD_* */
    point_t pt[BV_VLIST_CHUNK];	/**< @brief associated 3-point/vect */
};
#define BV_VLIST_NULL	((struct bv_vlist *)0)
#define BV_CK_VLIST(_p) BU_CKMAG((_p), BV_VLIST_MAGIC, "bv_vlist")

/* should these be enum? -Erik */
/* Values for cmd[] */
#define BV_VLIST_LINE_MOVE		0	/**< @brief specify new line */
#define BV_VLIST_LINE_DRAW		1	/**< @brief subsequent line vertex */
#define BV_VLIST_POLY_START		2	/**< @brief pt[] has surface normal */
#define BV_VLIST_POLY_MOVE		3	/**< @brief move to first poly vertex */
#define BV_VLIST_POLY_DRAW		4	/**< @brief subsequent poly vertex */
#define BV_VLIST_POLY_END		5	/**< @brief last vert (repeats 1st), draw poly */
#define BV_VLIST_POLY_VERTNORM		6	/**< @brief per-vertex normal, for interpolation */
#define BV_VLIST_TRI_START		7	/**< @brief pt[] has surface normal */
#define BV_VLIST_TRI_MOVE		8	/**< @brief move to first triangle vertex */
#define BV_VLIST_TRI_DRAW		9	/**< @brief subsequent triangle vertex */
#define BV_VLIST_TRI_END		10	/**< @brief last vert (repeats 1st), draw poly */
#define BV_VLIST_TRI_VERTNORM		11	/**< @brief per-vertex normal, for interpolation */
#define BV_VLIST_POINT_DRAW		12	/**< @brief Draw a single point */
#define BV_VLIST_POINT_SIZE		13	/**< @brief specify point pixel size */
#define BV_VLIST_LINE_WIDTH		14	/**< @brief specify line pixel width */
#define BV_VLIST_DISPLAY_MAT		15	/**< @brief specify the model matrix */
#define BV_VLIST_MODEL_MAT		16	/**< @brief specify the display matrix */
#define BV_VLIST_CMD_MAX		16	/**< @brief Max command number */
/**
 * Applications that are going to use BV_ADD_VLIST and BV_GET_VLIST
 * are required to execute this macro once, on their _free_hd:
 * BU_LIST_INIT(&_free_hd);
 *
 * Note that BV_GET_VLIST and BV_FREE_VLIST are non-PARALLEL.
 */
#define BV_GET_VLIST(_free_hd, p) do {\
	(p) = BU_LIST_FIRST(bv_vlist, (_free_hd)); \
	if (BU_LIST_IS_HEAD((p), (_free_hd))) { \
	    BU_ALLOC((p), struct bv_vlist); \
	    (p)->l.magic = BV_VLIST_MAGIC; \
	} else { \
	    BU_LIST_DEQUEUE(&((p)->l)); \
	} \
	(p)->nused = 0; \
    } while (0)

/** Place an entire chain of bv_vlist structs on the freelist _free_hd */
#define BV_FREE_VLIST(_free_hd, hd) do { \
	BU_CK_LIST_HEAD((hd)); \
	BU_LIST_APPEND_LIST((_free_hd), (hd)); \
    } while (0)

#define BV_ADD_VLIST(_free_hd, _dest_hd, pnt, draw) do { \
	struct bv_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bv_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BV_VLIST_CHUNK) { \
	    BV_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	VMOVE(_vp->pt[_vp->nused], (pnt)); \
	_vp->cmd[_vp->nused++] = (draw); \
    } while (0)

/** Change the transformation matrix to display */
#define BV_VLIST_SET_DISP_MAT(_free_hd, _dest_hd, _ref_pt) do { \
	struct bv_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bv_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BV_VLIST_CHUNK) { \
	    BV_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	VMOVE(_vp->pt[_vp->nused], (_ref_pt)); \
	_vp->cmd[_vp->nused++] = BV_VLIST_DISPLAY_MAT; \
    } while (0)

/** Change the transformation matrix to model */
#define BV_VLIST_SET_MODEL_MAT(_free_hd, _dest_hd) do { \
	struct bv_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bv_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BV_VLIST_CHUNK) { \
	    BV_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	_vp->cmd[_vp->nused++] = BV_VLIST_MODEL_MAT; \
    } while (0)

/** Set a point size to apply to the vlist elements that follow. */
#define BV_VLIST_SET_POINT_SIZE(_free_hd, _dest_hd, _size) do { \
	struct bv_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bv_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BV_VLIST_CHUNK) { \
	    BV_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	_vp->pt[_vp->nused][0] = (_size); \
	_vp->cmd[_vp->nused++] = BV_VLIST_POINT_SIZE; \
    } while (0)

/** Set a line width to apply to the vlist elements that follow. */
#define BV_VLIST_SET_LINE_WIDTH(_free_hd, _dest_hd, _width) do { \
	struct bv_vlist *_vp; \
	BU_CK_LIST_HEAD(_dest_hd); \
	_vp = BU_LIST_LAST(bv_vlist, (_dest_hd)); \
	if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BV_VLIST_CHUNK) { \
	    BV_GET_VLIST(_free_hd, _vp); \
	    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
	} \
	_vp->pt[_vp->nused][0] = (_width); \
	_vp->cmd[_vp->nused++] = BV_VLIST_LINE_WIDTH; \
    } while (0)


BV_EXPORT extern size_t bv_vlist_cmd_cnt(struct bv_vlist *vlist);
BV_EXPORT extern int bv_vlist_bbox(struct bu_list *vlistp, point_t *bmin, point_t *bmax, size_t *length, int *dispmode);



/**
 * For plotting, a way of separating plots into separate color vlists:
 * blocks of vlists, each with an associated color.
 */
struct bv_vlblock {
    uint32_t magic;
    size_t nused;
    size_t max;
    long *rgb;		/**< @brief rgb[max] variable size array */
    struct bu_list *head;		/**< @brief head[max] variable size array */
    struct bu_list *free_vlist_hd;	/**< @brief where to get/put free vlists */
};
#define BV_CK_VLBLOCK(_p)	BU_CKMAG((_p), BV_VLBLOCK_MAGIC, "bv_vlblock")


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
BV_EXPORT extern void bv_vlist_3string(struct bu_list *vhead,
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
BV_EXPORT extern void bv_vlist_2string(struct bu_list *vhead,
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
BV_EXPORT extern const char *bv_vlist_get_cmd_description(int cmd);

/**
 * Validate an bv_vlist chain for having reasonable values inside.
 * Calls bu_bomb() if not.
 *
 * Returns -
 * npts Number of point/command sets in use.
 */
BV_EXPORT extern size_t bv_ck_vlist(const struct bu_list *vhead);


/**
 * Duplicate the contents of a vlist.  Note that the copy may be more
 * densely packed than the source.
 */
BV_EXPORT extern void bv_vlist_copy(struct bu_list *vlists,
				    struct bu_list *dest,
				    const struct bu_list *src);


/**
 * Convert a vlist chain into a blob of network-independent binary,
 * for transmission to another machine.
 *
 * The result is stored in a vls string, so that both the address and
 * length are available conveniently.
 */
BV_EXPORT extern void bv_vlist_export(struct bu_vls *vls,
				      struct bu_list *hp,
				      const char *name);


/**
 * Convert a blob of network-independent binary prepared by
 * vls_vlist() and received from another machine, into a vlist chain.
 */
BV_EXPORT extern void bv_vlist_import(struct bu_list *vlists,
				      struct bu_list *hp,
				      struct bu_vls *namevls,
				      const unsigned char *buf);

BV_EXPORT extern void bv_vlist_cleanup(struct bu_list *hd);

BV_EXPORT extern struct bv_vlblock *bv_vlblock_init(struct bu_list *free_vlist_hd, /* where to get/put free vlists */
						    int max_ent);

BV_EXPORT extern void bv_vlblock_free(struct bv_vlblock *vbp);

BV_EXPORT extern struct bu_list *bv_vlblock_find(struct bv_vlblock *vbp,
						 int r,
						 int g,
						 int b);


BV_EXPORT void bv_vlist_rpp(struct bu_list *vlists, struct bu_list *hd, const point_t minn, const point_t maxx);

/**
 * Output a bv_vlblock object in extended UNIX-plot format, including
 * color.
 */
BV_EXPORT extern void bv_plot_vlblock(FILE *fp,
				      const struct bv_vlblock *vbp);

BV_EXPORT extern void bv_vlblock_to_objs(struct bu_ptbl *out,
                                     const char *name_root,
				     struct bv_vlblock *vbp,
				     struct bview *v,
				     struct bv_scene_obj *f,
				     struct bu_list *vlfree);


BV_EXPORT extern struct bv_scene_obj *
bv_vlblock_obj(struct bv_vlblock *vbp, struct bview *v, const char *name);

/**
 * Output a vlist as an extended 3-D floating point UNIX-Plot file.
 * You provide the file.  Uses libplot3 routines to create the
 * UNIX-Plot output.
 */
BV_EXPORT extern void bv_vlist_to_uplot(FILE *fp,
					const struct bu_list *vhead);

/** @} */

__END_DECLS

#endif  /* BV_VLIST_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
