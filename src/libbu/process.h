/*                      P R O C E S S . H
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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

/* Private libbu process related definitions shared between multiple files */

#ifndef LIBBU_PROCESS_H
#define LIBBU_PROCESS_H

/* Apparently the termination code on Windows is arbitrary and somewhat
 * ill-defined - see https://bugs.python.org/issue31863 for more info.  Try our
 * own unlikely return number in the hopes of being relatively reliable for
 * abort identification. */
#define BU_MSVC_ABORT_EXIT 97101  /* ascii - a == 97 && e == 101 */

#endif /* LIBBU_PROCESS_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
