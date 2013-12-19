/*                      P A R A L L E L . H
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#ifndef LIBBU_PARALLEL_H
#define LIBBU_PARALLEL_H

#include "bu.h"

/**
 * Set affinity mask of current thread to the CPU set it is currently
 * running on. If it is not running on any CPUs in the set, it is
 * migrated to CPU 0 by default.
 *
 * Return:
 *  0 on Suceess
 * -1 on Failure
 *
 */
extern int parallel_set_affinity(int cpu);

extern void thread_set_cpu(int cpu);
extern int thread_get_cpu(void);

#endif /* LIBBU_PARALLEL_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
