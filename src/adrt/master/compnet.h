/*                       C O M P N E T . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2016 United States Government as represented by
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
/** @file compnet.h
 *
 */

#ifndef ADRT_MASTER_COMPNET_H
#define ADRT_MASTER_COMPNET_H

#include "common.h"

#define ISST_COMPNET_PORT 1983

void compnet_connect(char *host, int port);
void compnet_update(char *string, char status);
void compnet_reset();

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
