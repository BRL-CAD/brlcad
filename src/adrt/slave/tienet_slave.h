/*                  T I E N E T _ S L A V E . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2011 United States Government as represented by
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
/** @file tienet_slave.h
 *
 *  Comments -
 *      TIE Networking Slave Header
 *
 */
#ifndef _TIENET_SLAVE_H
#define _TIENET_SLAVE_H

extern	void	tienet_slave_init(int port, char *host, void fcb_work(tienet_buffer_t *buffer, tienet_buffer_t *result),
				  void fcb_free(void),
				  int ver_key);
extern	void	tienet_slave_free();

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
