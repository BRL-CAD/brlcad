/*                     U T I L . H
 *
 * @file util.h
 *
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
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
