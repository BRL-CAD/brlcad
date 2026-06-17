/*                        V L I S T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup libbsg
 *
 * @brief
 * Canonical vlist type, constants, macros and helpers for the BSG
 * scene graph.
 *
 * This header is the authoritative definition of the vlist type.
 * bv/vlist.h is a temporary backward-compatibility bridge that will be
 * removed once all callers have migrated to bsg/vlist.h.
 */
/** @{ */
/* @file bsg/vlist.h */

#ifndef BSG_VLIST_H
#define BSG_VLIST_H

#include "common.h"
#include <stdio.h>
#include "vmath.h"
#include "bu/magic.h"
#include "bu/list.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "bsg/defines.h"

__BEGIN_DECLS

/* -----------------------------------------------------------------------
 * Core vlist struct and type
 * ----------------------------------------------------------------------- */

#define BSG_VLIST_CHUNK 35   /**< @brief 32-bit mach => just less than 1k */

struct bsg_vlist {
    struct bu_list l;               /**< @brief magic, forw, back */
    size_t nused;                   /**< @brief elements 0..nused active */
    int cmd[BSG_VLIST_CHUNK];       /**< @brief BSG_VLIST_* command codes */
    point_t pt[BSG_VLIST_CHUNK];    /**< @brief associated 3-point/vect */
};
typedef struct bsg_vlist bsg_vlist;

#define BSG_VLIST_NULL  ((bsg_vlist *)0)
#define BSG_VLIST_MAGIC 0x98237474
#define BSG_CK_VLIST(_p) BU_CKMAG((_p), BSG_VLIST_MAGIC, "bsg_vlist")

/* -----------------------------------------------------------------------
 * Vlist command constants
 * ----------------------------------------------------------------------- */
#define BSG_VLIST_LINE_MOVE         0   /**< @brief specify new line */
#define BSG_VLIST_LINE_DRAW         1   /**< @brief subsequent line vertex */
#define BSG_VLIST_POLY_START        2   /**< @brief pt[] has surface normal */
#define BSG_VLIST_POLY_MOVE         3   /**< @brief move to first poly vertex */
#define BSG_VLIST_POLY_DRAW         4   /**< @brief subsequent poly vertex */
#define BSG_VLIST_POLY_END          5   /**< @brief last vert (repeats 1st), draw poly */
#define BSG_VLIST_POLY_VERTNORM     6   /**< @brief per-vertex normal */
#define BSG_VLIST_TRI_START         7   /**< @brief pt[] has surface normal */
#define BSG_VLIST_TRI_MOVE          8   /**< @brief move to first triangle vertex */
#define BSG_VLIST_TRI_DRAW          9   /**< @brief subsequent triangle vertex */
#define BSG_VLIST_TRI_END           10  /**< @brief last vert (repeats 1st), draw poly */
#define BSG_VLIST_TRI_VERTNORM      11  /**< @brief per-vertex normal */
#define BSG_VLIST_POINT_DRAW        12  /**< @brief Draw a single point */
#define BSG_VLIST_POINT_SIZE        13  /**< @brief specify point pixel size */
#define BSG_VLIST_LINE_WIDTH        14  /**< @brief specify line pixel width */
#define BSG_VLIST_DISPLAY_MAT       15  /**< @brief specify the model matrix */
#define BSG_VLIST_MODEL_MAT         16  /**< @brief specify the display matrix */
#define BSG_VLIST_CMD_MAX           16  /**< @brief Max command number */

/* -----------------------------------------------------------------------
 * Vlist operation macros
 *
 * Applications must call BU_LIST_INIT on their free-list head before
 * using BSG_GET_VLIST / BSG_ADD_VLIST.  These macros are non-PARALLEL.
 * ----------------------------------------------------------------------- */

#define BSG_GET_VLIST(_free_hd, p) do { \
(p) = BU_LIST_FIRST(bsg_vlist, (_free_hd)); \
if (BU_LIST_IS_HEAD((p), (_free_hd))) { \
    BU_ALLOC((p), bsg_vlist); \
    (p)->l.magic = BSG_VLIST_MAGIC; \
} else { \
    BU_LIST_DEQUEUE(&((p)->l)); \
} \
(p)->nused = 0; \
    } while (0)

/** Place an entire chain of bsg_vlist structs on the freelist _free_hd */
#define BSG_FREE_VLIST(_free_hd, hd) do { \
BU_CK_LIST_HEAD((hd)); \
BU_LIST_APPEND_LIST((_free_hd), (hd)); \
    } while (0)

#define BSG_ADD_VLIST(_free_hd, _dest_hd, pnt, draw) do { \
bsg_vlist *_vp; \
BU_CK_LIST_HEAD(_dest_hd); \
_vp = BU_LIST_LAST(bsg_vlist, (_dest_hd)); \
if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BSG_VLIST_CHUNK) { \
    BSG_GET_VLIST(_free_hd, _vp); \
    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
} \
VMOVE(_vp->pt[_vp->nused], (pnt)); \
_vp->cmd[_vp->nused++] = (draw); \
    } while (0)

/** Change the transformation matrix to display */
#define BSG_VLIST_SET_DISP_MAT(_free_hd, _dest_hd, _ref_pt) do { \
bsg_vlist *_vp; \
BU_CK_LIST_HEAD(_dest_hd); \
_vp = BU_LIST_LAST(bsg_vlist, (_dest_hd)); \
if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BSG_VLIST_CHUNK) { \
    BSG_GET_VLIST(_free_hd, _vp); \
    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
} \
VMOVE(_vp->pt[_vp->nused], (_ref_pt)); \
_vp->cmd[_vp->nused++] = BSG_VLIST_DISPLAY_MAT; \
    } while (0)

/** Change the transformation matrix to model */
#define BSG_VLIST_SET_MODEL_MAT(_free_hd, _dest_hd) do { \
bsg_vlist *_vp; \
BU_CK_LIST_HEAD(_dest_hd); \
_vp = BU_LIST_LAST(bsg_vlist, (_dest_hd)); \
if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BSG_VLIST_CHUNK) { \
    BSG_GET_VLIST(_free_hd, _vp); \
    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
} \
_vp->cmd[_vp->nused++] = BSG_VLIST_MODEL_MAT; \
    } while (0)

/** Set a point size to apply to vlist elements that follow */
#define BSG_VLIST_SET_POINT_SIZE(_free_hd, _dest_hd, _size) do { \
bsg_vlist *_vp; \
BU_CK_LIST_HEAD(_dest_hd); \
_vp = BU_LIST_LAST(bsg_vlist, (_dest_hd)); \
if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BSG_VLIST_CHUNK) { \
    BSG_GET_VLIST(_free_hd, _vp); \
    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
} \
_vp->pt[_vp->nused][0] = (_size); \
_vp->cmd[_vp->nused++] = BSG_VLIST_POINT_SIZE; \
    } while (0)

/** Set a line width to apply to vlist elements that follow */
#define BSG_VLIST_SET_LINE_WIDTH(_free_hd, _dest_hd, _width) do { \
bsg_vlist *_vp; \
BU_CK_LIST_HEAD(_dest_hd); \
_vp = BU_LIST_LAST(bsg_vlist, (_dest_hd)); \
if (BU_LIST_IS_HEAD(_vp, (_dest_hd)) || _vp->nused >= BSG_VLIST_CHUNK) { \
    BSG_GET_VLIST(_free_hd, _vp); \
    BU_LIST_INSERT((_dest_hd), &(_vp->l)); \
} \
_vp->pt[_vp->nused][0] = (_width); \
_vp->cmd[_vp->nused++] = BSG_VLIST_LINE_WIDTH; \
    } while (0)

/* -----------------------------------------------------------------------
 * BSG vlist API
 * ----------------------------------------------------------------------- */

/**
 * Count the total number of vlist commands in @p vlist.
 * Returns 0 when @p vlist is NULL.
 */
BSG_EXPORT extern size_t
bsg_vlist_cmd_cnt(bsg_vlist *vlist);

/**
 * Duplicate the contents of a vlist.  The copy may be more densely
 * packed than the source.
 */
BSG_EXPORT extern void
bsg_vlist_copy(struct bu_list *vlists,
               struct bu_list *dest,
               const struct bu_list *src);

/**
 * Emit wireframe vlist commands for an ARB8 defined by the eight
 * points @p pts into @p vhead, using @p vlfree as the free-list
 * allocator.  Produces exactly 18 commands.
 */
BSG_EXPORT extern void
bsg_vlist_arb8(struct bu_list *vhead, struct bu_list *vlfree, point_t pts[8]);

/* -----------------------------------------------------------------------
 * vlblock — colored vlist set
 * ----------------------------------------------------------------------- */

struct bsg_vlblock {
    uint32_t magic;
    size_t nused;
    size_t max;
    long *rgb;                      /**< @brief rgb[max] variable size array */
    struct bu_list *head;           /**< @brief head[max] variable size array */
    struct bu_list *free_vlist_hd;  /**< @brief where to get/put free vlists */
};
#define BSG_VLBLOCK_MAGIC 0x981bd112
#define BSG_CK_VLBLOCK(_p) BU_CKMAG((_p), BSG_VLBLOCK_MAGIC, "bsg_vlblock")

/* bsg_vlist_* and bsg_vlblock_* function declarations (still bv_ prefixed until rename slice) */
BSG_EXPORT extern size_t bsg_vlist_cmd_cnt(bsg_vlist *vlist);
BSG_EXPORT extern int bsg_vlist_bbox(struct bu_list *vlistp, point_t *bmin, point_t *bmax, size_t *length, int *dispmode);
BSG_EXPORT extern void bsg_vlist_3string(struct bu_list *vhead, struct bu_list *free_hd, const char *string, const point_t origin, const mat_t rot, double scale);
BSG_EXPORT extern void bsg_vlist_2string(struct bu_list *vhead, struct bu_list *free_hd, const char *string, double x, double y, double scale, double theta);
BSG_EXPORT extern const char *bsg_vlist_get_cmd_description(int cmd);
BSG_EXPORT extern size_t bsg_ck_vlist(const struct bu_list *vhead);
BSG_EXPORT extern void bsg_vlist_copy(struct bu_list *vlists, struct bu_list *dest, const struct bu_list *src);
BSG_EXPORT extern void bsg_vlist_export(struct bu_vls *vls, struct bu_list *hp, const char *name);
BSG_EXPORT extern void bsg_vlist_import(struct bu_list *vlists, struct bu_list *hp, struct bu_vls *namevls, const unsigned char *buf);
BSG_EXPORT extern void bsg_vlist_cleanup(struct bu_list *hd);
BSG_EXPORT extern struct bsg_vlblock *bsg_vlblock_init(struct bu_list *free_vlist_hd, int max_ent);
BSG_EXPORT extern void bsg_vlblock_free(struct bsg_vlblock *vbp);
BSG_EXPORT extern struct bu_list *bsg_vlblock_find(struct bsg_vlblock *vbp, int r, int g, int b);
BSG_EXPORT void bsg_vlist_rpp(struct bu_list *vlists, struct bu_list *hd, const point_t minn, const point_t maxx);
BSG_EXPORT extern void bsg_plot_vlblock(FILE *fp, const struct bsg_vlblock *vbp);
BSG_EXPORT extern void bsg_vlist_to_uplot(FILE *fp, const struct bu_list *vhead);

__END_DECLS

#endif /* BSG_VLIST_H */

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
