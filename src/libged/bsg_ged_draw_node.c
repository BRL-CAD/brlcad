/*             B S G _ G E D _ D R A W _ N O D E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libged/bsg_ged_draw_node.c
 *
 * Remaining scene-level helpers for GED draw overlays and release.
 */

#include "common.h"

#include "bsg/scene_builder.h"
#include "ged/bsg_ged_draw.h"
#include "./ged_private.h"
#include "./bsg_ged_draw_private.h"


static void
_ged_draw_scene_ref_release_data_recurse(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return;

    for (size_t i = 0; i < bsg_scene_child_count(ref); i++)
	_ged_draw_scene_ref_release_data_recurse(bsg_scene_child_at(ref, i));
    ged_draw_scene_ref_highlight_free_cb(ref);
}


void
ged_draw_scene_ref_release(bsg_scene_ref ref)
{
    if (bsg_scene_ref_is_null(ref))
	return;
    _ged_draw_scene_ref_release_data_recurse(ref);
    bsg_scene_ref_destroy(ref);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
