/*                       S E L E C T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @addtogroup ged_select
 *
 * Geometry EDiting Library Object Selection Functions.
 *
 */
/** @{ */
/** @file ged/view/select.h */

#ifndef GED_VIEW_SELECT_H
#define GED_VIEW_SELECT_H

#include "common.h"
#include "ged/defines.h"

__BEGIN_DECLS


/**
 * Returns a list of items within the previously defined rectangle.
 */
GED_EXPORT extern int ged_rselect(struct ged *gedp, int argc, const char *argv[]);

/**
 * Returns a list of items within the specified rectangle or circle.
 */
GED_EXPORT extern int ged_select(struct ged *gedp, int argc, const char *argv[]);

/**
 * Return ged selections for specified object. Created if it doesn't
 * exist.
 */
GED_EXPORT struct rt_object_selections *ged_get_object_selections(struct ged *gedp,
								  const char *object_name);

/**
 * Return ged selections of specified kind for specified object.
 * Created if it doesn't exist.
 */
GED_EXPORT struct rt_selection_set *ged_get_selection_set(struct ged *gedp,
							  const char *object_name,
							  const char *selection_name);




__END_DECLS

#endif /* GED_VIEW_SELECT_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
