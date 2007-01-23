/*                   T I E N E T _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#ifndef _TIENET_UTIL_H
#define _TIENET_UTIL_H


#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>


typedef struct tienet_sem_s {
  int val;
  pthread_mutex_t mut;
  pthread_cond_t cond;
} tienet_sem_t;


extern	void	tienet_flip(void *src, void *dest, int size);
extern	int	tienet_send(int socket, void *data, int size, int flip);
extern	int	tienet_recv(int socket, void *data, int size, int flip);

extern	void	tienet_sem_init(tienet_sem_t *sem, int val);
extern	void	tienet_sem_free(tienet_sem_t *sem);
extern	void	tienet_sem_wait(tienet_sem_t *sem);
extern	void	tienet_sem_post(tienet_sem_t *sem);

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
