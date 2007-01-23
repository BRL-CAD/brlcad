/*                    D I S P A T C H E R . H
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file dispatcher.h
 *
 * Author -
 *   Justin Shumaker
 *
 */

#ifndef _ISST_DISPATCHER_H
#define _ISST_DISPATCHER_H

#include "cdb.h"

extern	void	isst_dispatcher_init();
extern	void	isst_dispatcher_free();
extern	void	isst_dispatcher_generate(common_db_t *db, void *data, int data_len);


#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
