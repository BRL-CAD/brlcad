/*                        T I E N E T . C
 * BRL-CAD
 *
 * Copyright (c) 2016 United States Government as represented by
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

#include "tienet.h"

#include "bnetwork.h"
#include "bio.h"

int
tienet_send(int tsocket, void* data, size_t size)
{
    fd_set	 set;
    unsigned int ind = 0;
    int		 r;

    FD_ZERO(&set);
    FD_SET(tsocket, &set);

    do {
	select(tsocket+1, NULL, &set, NULL, NULL);
	r = write(tsocket, &((char*)data)[ind], size-ind);
	ind += r;
	if (r <= 0) return 1;	/* Error, socket is probably dead */
    } while (ind < size);

    return 0;
}

int
tienet_recv(int tsocket, void* data, size_t size)
{
    fd_set	 set;
    unsigned int ind = 0;
    int		 r;

    FD_ZERO(&set);
    FD_SET(tsocket, &set);

    do {
	select(tsocket+1, NULL, &set, NULL, NULL);
	r = read(tsocket, &((char*)data)[ind], size-ind);
	ind += r;
	if (r <= 0) return 1;	/* Error, socket is probably dead */
    } while (ind < size);

    return 0;
}


void tienet_sem_init(tienet_sem_t *sem, int val)
{
    mtx_init(&sem->mut, 0);
    cnd_init(&sem->cond);
    sem->val = val;
}


void tienet_sem_free(tienet_sem_t *sem)
{
    mtx_destroy(&sem->mut);
    cnd_destroy(&sem->cond);
}


void tienet_sem_post(tienet_sem_t *sem)
{
    mtx_lock(&sem->mut);
    sem->val++;
    cnd_signal(&sem->cond);
    mtx_unlock(&sem->mut);
}


void tienet_sem_wait(tienet_sem_t *sem)
{
    mtx_lock(&sem->mut);
    if (!sem->val)
	cnd_wait(&sem->cond, &sem->mut);
    sem->val--;
    mtx_unlock(&sem->mut);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
