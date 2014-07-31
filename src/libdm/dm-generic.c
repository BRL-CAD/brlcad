/*                    D M - G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2014 United States Government as represented by
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
/** @file libdm/dm-generic.c
 *
 * Generic display manager routines.
 *
 */

#include "common.h"
#include "bio.h"

#include <string.h>

#include "tcl.h"

#include "bu.h"
#include "vmath.h"
#include "dm.h"

void
dm_set_perspective(struct dm *dmp, int perspective_flag)
{
    if (!dmp) return;
    dmp->perspective = perspective_flag;
}

int dm_get_perspective(struct dm *dmp) 
{
    return dmp->perspective;
}

void
dm_set_width(struct dm *dmp, int width)
{
    if (!dmp) return;
    dmp->with = width;
}

int dm_get_width(struct dm *dmp) 
{
    return dmp->width;
}

void
dm_set_height(struct dm *dmp, int height)
{
    if (!dmp) return;
    dmp->with = height;
}

int dm_get_height(struct dm *dmp) 
{
    return dmp->height;
}

void
dm_set_proj_matrix(struct dm *dmp, mat_t pmat)
{
    if (!dmp) return;
    MAT_COPY(dmp->proj_matrix, pmat);
}

matp_t dm_get_proj_mat(struct dm *dmp) 
{
    return &(dmp->proj_matrix);
}

void
dm_set_view_matrix(struct dm *dmp, mat_t vmat)
{
    if (!dmp) return;
    MAT_COPY(dmp->view_matrix, vmat);
}

matp_t dm_get_view_mat(struct dm *dmp) 
{
    return &(dmp->view_matrix);
}

void
dm_set_background_rgb(struct dm *dmp,  unsigned char r, unsigned char g, unsigned char b)
{
    if (!dmp) return;
    dmp->dm_bg[0] = r;
    dmp->dm_bg[1] = g;
    dmp->dm_bg[2] = b;
}

unsigned char *dm_get_background_rgb(struct dm *dmp) 
{
    return dmp->dm_bg;
}


void
dm_set_foreground_rgb(struct dm *dmp,  unsigned char r, unsigned char g, unsigned char b)
{
    if (!dmp) return;
    dmp->dm_fg[0] = r;
    dmp->dm_fg[1] = g;
    dmp->dm_fg[2] = b;
}

unsigned char *dm_get_foreground_rgb(struct dm *dmp) 
{
    return dmp->dm_fg;
}

void
dm_set_default_draw_width(struct dm *dmp, fastf_t draw_width)
{
    if (!dmp) return;
    dmp->draw_width = draw_width;
}

fastf_t dm_get_default_draw_width(struct dm *dmp)
{
    return dmp->draw_width;
}

void
dm_set_default_fontsize(struct dm *dmp, int fontsize)
{
    if (!dmp) return;
    dmp->fontsize= fontsize;
}

fastf_t dm_get_default_fontsize(struct dm *dmp)
{
    return dmp->fontsize;
}

struct bu_attribute_value_set *
dm_get_settings(struct dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->dm_extra_settings;
}

int
dm_set_setting(struct dm *dmp, const char *key, const char *val)
{
    if (!dmp) return -1;
    if !(dmp->dm_extra_settings) return -1;

    if (!bu_avs_get(dmp->dm_extra_settings, key)) return -1;

    return !bu_avs_add(dmp->dm_extra_settings, key, val);
}

const char *
dm_get_setting(struct dm *dmp, const char *key, const char *val)
{
    if (!dmp) return -1;
    if !(dmp->dm_extra_settings) return -1;

    return bu_avs_get(dmp->dm_extra_settings, key);
}


struct dm_display_list *
dm_obj_add(struct dm *dmp, const char *handle, int style_type, struct bn_vlist *vlist, struct bu_ptbl *obj_set)
{
}

struct dm_display_list *
dm_obj_find(struct dm *dmp, const char *handle)
{
}

void                    
dm_obj_remove(struct dm *dmp, const char *handle)
{
}


void
dm_set_obj_localmat(struct dm *dmp, const char *handle, mat_t matrix)
{
}

matp_t
dm_get_obj_localmat(struct dm *dmp, const char *handle)
{
}

void   
dm_set_obj_rgb(struct dm *dmp, const char *handle, unsigned char r, unsigned char g, unsigned char b)
{
}

unsigned char *
dm_get_obj_rgb(struct dm *dmp, const char *handle)
{
}

void
dm_set_obj_draw_width(struct dm *dmp, const char *handle, fastf_t draw_width)
{
}

fastf_t 
dm_get_obj_draw_width(struct dm *dmp, const char *handle)
{
}

void
dm_set_obj_fontsize(struct dm *dmp, const char *handle, int fontsize)
{
}

int
dm_get_obj_fontsize(struct dm *dmp, const char *handle)
{
}

void
dm_set_obj_dirty(struct dm *dmp, const char *handle, int flag)
{
}

int
dm_get_obj_dirty(struct dm *dmp, const char *handle)
{
}

void
dm_set_obj_visible(struct dm *dmp, const char *handle, int flag)
{
}

int
dm_get_obj_visible(struct dm *dmp, const char *handle)
{
}


void
dm_set_obj_highlight(struct dm *dmp, const char *handle, int flag)
{
}

int
dm_get_obj_highlight(struct dm *dmp, const char *handle)
{
}


struct bu_attribute_value_set *
dm_get_obj_settings(struct dm *dmp, const char *handle)
{
}

int
dm_set_obj_setting(struct dm *dmp, const char *handle, const char *key, const char *val)
{
}

const char *
dm_get_obj_setting(struct dm *dmp, const char *handle, const char *key)
{
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
