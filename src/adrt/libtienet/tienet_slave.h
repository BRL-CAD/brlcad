/*                     S L A V E . H
 *
 * @file slave.h
 *
 * BRL-CAD
 *
 * Copyright (c) 2002-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 *  Comments -
 *      TIE Networking Slave Header
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#ifndef _TIENET_SLAVE_H
#define _TIENET_SLAVE_H


#include "tie.h"


extern	void	tienet_slave_init(int port, char *host, void fcb_init(tie_t *tie, int socknum),
                                                        void fcb_work(tie_t *tie, void *data, unsigned int size, void **res_data, unsigned int *res_size),
                                                        void fcb_free(void),
                                                        void fcb_mesg(void *mesg, unsigned int mesg_len),
                                                        int ver_key);
extern	void	tienet_slave_free();
extern	short	tienet_endian;


#endif
