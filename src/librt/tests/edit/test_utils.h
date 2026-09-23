/*                    T E S T _ U T I L S . H
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

#ifndef RT_EDIT_TEST_UTILS_H
#define RT_EDIT_TEST_UTILS_H

#include "common.h"

#include "bu/defines.h"

static inline int
edit_test_filename_callback(int UNUSED(argc), const char **UNUSED(argv),
                            void *data, void *result)
{
    *(const char **)result = (const char *)data;
    return BRLCAD_OK;
}

#endif /* RT_EDIT_TEST_UTILS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
