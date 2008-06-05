/*                   T I E N E T _ U T I L . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2002-2008 United States Government as represented by
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
/** @file tienet_util.h
 *
 *  Comments -
 *      TIE Networking Utilities Header
 *
 */

#ifndef _TIENET_UTIL_H
#define _TIENET_UTIL_H

#include "common.h"

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_STDINT_H
#  include <stdint.h>
#endif

#include "bu.h"

#define TIENET_BUFFER_INIT(_b) { \
	_b.data = NULL; \
	_b.size = 0; \
	_b.ind = 0; }

#define TIENET_BUFFER_FREE(_b) bu_free(_b.data, "tienet buffer");

#define TIENET_BUFFER_SIZE(_b, _s) { \
	if (_s > _b.size) { \
	  _b.data = bu_realloc(_b.data, _s, "tienet buffer size"); \
	  _b.size = _s; \
        } }

typedef struct tienet_buffer_s {
    uint8_t *data;
    uint32_t size;
    uint32_t ind;
} tienet_buffer_t;


typedef struct tienet_sem_s {
    int val;
    pthread_mutex_t mut;
    pthread_cond_t cond;
} tienet_sem_t;


extern	void	tienet_flip(void* src, void* dest, size_t size);
extern	int	tienet_send(int socket, void* data, size_t size);
extern	int	tienet_recv(int socket, void *data, int size);

extern	void	tienet_sem_init(tienet_sem_t *sem, int val);
extern	void	tienet_sem_free(tienet_sem_t *sem);
extern	void	tienet_sem_wait(tienet_sem_t *sem);
extern	void	tienet_sem_post(tienet_sem_t *sem);

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
