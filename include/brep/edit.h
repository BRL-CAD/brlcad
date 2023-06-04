/*                       B O O L E A N . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2023 United States Government as represented by
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
/** @addtogroup brep_edit
 * @brief
 * Implementation of edit support for brep.
 */
#ifndef BREP_EDIT_H
#define BREP_EDIT_H

#include "common.h"
#include "brep/defines.h"
#include "rt/geom.h"

/** @{ */
/** @file brep/edit.h */
__BEGIN_DECLS

/**
 * create an rt_brep_internal structure with empty brep
 */
BREP_EXPORT extern void *
brep_create_empty_brep();

__END_DECLS

#endif  /* BREP_EDIT_H */
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
