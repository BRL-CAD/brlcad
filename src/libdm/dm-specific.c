/*                   D M - S P E C I F I C . C
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
/** @file libdm/dm-specific.c
 *
 * Display manager routines that need specific knowledge of the
 * underlying drawing system.
 *
 */

#include "common.h"
#include "bio.h"

#include <string.h>

#include "dm.h"

/* TODO - this begs for a plugin based system - should not
 * be ifdef wrapping so much logic. */

/* Include local headers for system specific routines */

#include "txt/dm_txt.h"
#ifdef DM_QT
#  include "qt/dm_qt.h"
#endif
#ifdef DM_OSG
#  include "osg/dm_osg.h"
#endif

int
dm_init(struct dm *dm)
{
    if (!dm) return -1;
    if (dm->is_embedded && !dm->parent_info && !dm->type == DM_TYPE_NULL) return -1;

    switch (dm->type) {
	case DM_TYPE_NULL:
	    dm->dm_canvas = NULL;
	    return 0;
	case DM_TYPE_TXT:
	    return txt_init(dm);
#ifdef(DM_QT)
	case DM_TYPE_QT:
	    return qt_init(dm);
#endif
#ifdef DM_OSG
	case DM_TYPE_OSG:
	    return osg_init(dm);
#endif
	default:
	    break;
    }

    return -1;
}


int
dm_close(struct dm *dm)
{
    if (!dm) return -1;
    switch (dm->type) {
	case DM_TYPE_NULL:
	    dm->dm_canvas = NULL;
	    return 0;
	case DM_TYPE_TXT:
	    return txt_close(dm);
#ifdef(DM_QT)
	case DM_TYPE_QT:
	    return qt_close(dm);
#endif
#ifdef DM_OSG
	case DM_TYPE_OSG:
	    return osg_close(dm);
#endif
	default:
	    break;
    }

    return -1;
}

int
dm_refresh(struct dm *dm)
{
    if (!dm) return -1;
    switch (dm->type) {
	case DM_TYPE_NULL:
	    return 0;
	case DM_TYPE_TXT:
	    return txt_refresh(dm);
#ifdef(DM_QT)
	case DM_TYPE_QT:
	    return qt_refresh(dm);
#endif
#ifdef DM_OSG
	case DM_TYPE_OSG:
	    return osg_refresh(dm);
#endif
	default:
	    break;
    }

    return -1;
}

void *
dm_canvas(struct dm *dm)
{
    if (!dm) return NULL;
    switch (dm->type) {
	case DM_TYPE_NULL:
	    return NULL;
	case DM_TYPE_TXT:
	    return txt_canvas(dm); /* Probably a pointer to a bu_vls */
#ifdef(DM_QT)
	case DM_TYPE_QT:
	    return qt_canvas(dm); /* Some Qt drawing context */
#endif
#ifdef DM_OSG
	case DM_TYPE_OSG:
	    return osg_canvas(dm); /* Probably either the osgViewer or the raw OpenGL context */
#endif
	default:
	    break;
    }

    return -1;
}

int
dm_change_type(struct dm *dm, int dm_t)
{
    if (!dm) return -1;
    /* If we're already the requested type, do nothing */
    if (dm->type == dm_t) return 0;
    if (!dm_close(dm)) return -1;
    dm->type = dm_t;
    return dm_init(dm);
}

int
dm_get_image(struct dm *dm, icv_image_t *image)
{
    if (!dm || !image) return -1;
    switch (dm->type) {
	case DM_TYPE_NULL:
	    return 0;
	case DM_TYPE_TXT:
	    return 0;
#ifdef(DM_QT)
	case DM_TYPE_QT:
	    return qt_get_image(dm, image);
#endif
#ifdef DM_OSG
	case DM_TYPE_OSG:
	    return osg_get_image(dm, image);
#endif
	default:
	    break;
    }

    return -1;
}

int
dm_get_obj_image(struct dm *dm, const char *handle, icv_image_t *image)
{
    if (!dm || !image) return -1;
    switch (dm->type) {
	case DM_TYPE_NULL:
	    return 0;
	case DM_TYPE_TXT:
	    return 0;
#ifdef(DM_QT)
	case DM_TYPE_QT:
	    return qt_get_obj_image(dm, handle, image);
#endif
#ifdef DM_OSG
	case DM_TYPE_OSG:
	    return osg_get_obj_image(dm, handle, image);
#endif
	default:
	    break;
    }

    return -1;
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
