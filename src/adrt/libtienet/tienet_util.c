/*                   T I E N E T _ U T I L . C
 * BRL-CAD / ADRT
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
/** @file tienet_util.c
 *
 *  Comments -
 *      TIE Networking Utilities
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

#include <unistd.h>
#include <sys/select.h>
#include "tienet_util.h"


void tienet_flip(void *src, void *dest, int size) {
  int   i;
  char  b;
  for(i = 0; i < size/2; i++) {
    b = ((char*)src)[i];
    ((char*)dest)[i] = ((char*)src)[size-i-1];
    ((char*)dest)[size-i-1] = b;
  }
}


int tienet_send(int socket, void *data, int size, int flip) {
  fd_set	set;
  int		ind = 0, r;

  FD_ZERO(&set);
  FD_SET(socket, &set);

  if(flip)
    tienet_flip(data, data, size);

  do {
    select(socket+1, NULL, &set, NULL, NULL);
    ind += r = write(socket, &((char*)data)[ind], size-ind);
    if(r <= 0) return(1);	/* Error, socket is probably dead */
  } while(ind < size);

  return(0);
}


int tienet_recv(int socket, void *data, int size, int flip) {
  fd_set	set;
  int		ind = 0, r;

  FD_ZERO(&set);
  FD_SET(socket, &set);

  do {
    select(socket+1, &set, NULL, NULL, NULL);
    ind += r = read(socket, &((char*)data)[ind], size-ind);
    if(r <= 0) {
      perror("READ_ERROR");
      return(1);	/* Error, socket is probably dead */
    }
  } while(ind < size);

  if(flip)
    tienet_flip(data, data, size);

  return(0);
}


void tienet_sem_init(tienet_sem_t *sem, int val) {
  pthread_mutex_init(&sem->mut, 0);
  pthread_cond_init(&sem->cond, 0);
  sem->val = val;
}


void tienet_sem_free(tienet_sem_t *sem) {
  pthread_mutex_destroy(&sem->mut);
  pthread_cond_destroy(&sem->cond);
}


void tienet_sem_post(tienet_sem_t *sem) {
  pthread_mutex_lock(&sem->mut);
  sem->val++;
  pthread_cond_signal(&sem->cond);
  pthread_mutex_unlock(&sem->mut);
}


void tienet_sem_wait(tienet_sem_t *sem) {
  pthread_mutex_lock(&sem->mut);
  if(!sem->val)
    pthread_cond_wait(&sem->cond, &sem->mut);
  sem->val--;
  pthread_mutex_unlock(&sem->mut);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
