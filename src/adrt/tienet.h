/*                        T I E N E T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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

#ifndef ADRT_TIENET_H
#define ADRT_TIENET_H

#include "common.h"

#include "bu/malloc.h"
#include "bu/tc.h"

#define	TN_MASTER_PORT		1980
#define	TN_SLAVE_PORT		1981

#define	TN_OP_PREP		0x0010
#define	TN_OP_REQWORK		0x0011
#define	TN_OP_SENDWORK		0x0012
#define	TN_OP_RESULT		0x0013
#define	TN_OP_COMPLETE		0x0014
#define	TN_OP_SHUTDOWN		0x0015
#define TN_OP_OKAY		0x0016
#define	TN_OP_MESSAGE		0x0017

#define TIENET_BUFFER_INIT(_b) { \
	_b.data = NULL; \
	_b.size = 0; \
	_b.ind = 0; }

#define TIENET_BUFFER_FREE(_b) bu_free(_b.data, "tienet buffer");

#define TIENET_BUFFER_SIZE(_b, _s) { \
	if ((size_t)_s > (size_t)_b.size) { \
	    _b.data = (uint8_t *)bu_realloc(_b.data, (size_t)_s, "tienet buffer size");	\
	    _b.size = (uint32_t)_s; \
	} }

typedef struct tienet_buffer_s {
    uint8_t *data;
    uint32_t size;
    uint32_t ind;
} tienet_buffer_t;


typedef struct tienet_sem_s {
    int val;
    bu_mtx_t mut;
    bu_cnd_t cond;
} tienet_sem_t;


int tienet_send(int socket, void* data, size_t size);
int tienet_recv(int socket, void* data, size_t size);

void tienet_sem_init(tienet_sem_t *sem, int val);
void tienet_sem_free(tienet_sem_t *sem);
void tienet_sem_post(tienet_sem_t *sem);
void tienet_sem_wait(tienet_sem_t *sem);

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
