/*                    V I E W _ S E T S . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file bv_util.c
 *
 * Utility functions for operating on BRL-CAD view sets
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bv/defines.h"
#include "bv/view_sets.h"
#include "./bv_private.h"

void
bv_set_init(struct bview_set *s)
{
    BU_GET(s->i, struct bview_set_internal);
    BU_PTBL_INIT(&s->i->views);
    bu_ptbl_init(&s->i->shared_db_objs, 8, "db_objs init");
    bu_ptbl_init(&s->i->shared_view_objs, 8, "view_objs init");
    BU_LIST_INIT(&s->i->vlfree);
    /* init the solid list */
    BU_GET(s->i->free_scene_obj, struct bv_scene_obj);
    BU_LIST_INIT(&s->i->free_scene_obj->l);
}


#define FREE_BV_SCENE_OBJ(p, fp) { \
    BU_LIST_APPEND(fp, &((p)->l)); }

void
bv_set_free(struct bview_set *s)
{
    if (s->i) {
	// Note - it is the caller's responsibility to have freed any data
	// associated with the ged or its i->views in the u_data pointers.
	struct bview *gdvp;
	for (size_t i = 0; i < BU_PTBL_LEN(&s->i->views); i++) {
	    gdvp = (struct bview *)BU_PTBL_GET(&s->i->views, i);
	    bu_vls_free(&gdvp->gv_name);
	    if (gdvp->callbacks) {
		bu_ptbl_free(gdvp->callbacks);
		BU_PUT(gdvp->callbacks, struct bu_ptbl);
	    }
	    bu_free((void *)gdvp, "bv");
	}
	bu_ptbl_free(&s->i->views);

	bu_ptbl_free(&s->i->shared_db_objs);
	bu_ptbl_free(&s->i->shared_view_objs);

	// TODO - replace free_scene_obj with bu_ptbl
	struct bv_scene_obj *sp, *nsp;
	sp = BU_LIST_NEXT(bv_scene_obj, &s->i->free_scene_obj->l);
	while (BU_LIST_NOT_HEAD(sp, &s->i->free_scene_obj->l)) {
	    nsp = BU_LIST_PNEXT(bv_scene_obj, sp);
	    BU_LIST_DEQUEUE(&((sp)->l));
	    FREE_BV_SCENE_OBJ(sp, &s->i->free_scene_obj->l);
	    sp = nsp;
	}
	BU_PUT(s->i->free_scene_obj, struct bv_scene_obj);
    }

    // TODO - clean up vlfree
}

void
bv_set_add_view(struct bview_set *s, struct bview *v){
    if (!s || !v)
	return;

    bu_ptbl_ins(&s->i->views, (long *)v);
}

void
bv_set_rm_view(struct bview_set *s, struct bview *v){
    if (!s || !v)
	return;

    bu_ptbl_rm(&s->i->views, (long int *)v);
}


struct bu_ptbl *
bv_set_views(struct bview_set *s){
    if (!s)
	return NULL;

    return &s->i->views;
}

struct bu_ptbl *
bv_set_view_db_objs(struct bview *v)
{
    return &v->vset->i->shared_db_objs;
}

struct bu_ptbl *
bv_set_view_objs(struct bview *v)
{
    return &v->vset->i->shared_view_objs;
}

struct bview *
bv_set_find_view(struct bview_set *s, const char *vname)
{
    struct bview *v = NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->i->views); i++) {
	struct bview *tv = (struct bview *)BU_PTBL_GET(&s->i->views, i);
	if (BU_STR_EQUAL(bu_vls_cstr(&tv->gv_name), vname)) {
	    v = tv;
	    break;
	}
    }

    return v;
}

struct bv_scene_obj *
bv_set_fsos(struct bview_set *s)
{
    return s->i->free_scene_obj;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
