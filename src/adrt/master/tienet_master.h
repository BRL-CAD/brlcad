/*                 T I E N E T _ M A S T E R . H
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
/** @file tienet_master.h
 *
 *  Comments -
 *      TIE Networking Master Header
 *
 */

#ifndef _TIENET_MASTER_H
#define _TIENET_MASTER_H

typedef struct tienet_sem_s {
    int val;
    pthread_mutex_t mut;
    pthread_cond_t cond;
} tienet_sem_t;

extern void   	tienet_master_init(int port, void fcb_result(tienet_buffer_t *result), char *list, char *exec, int buffer_size, int ver_key, int verbose);
extern void	tienet_master_free();
extern void	tienet_master_push(const void *data, size_t size);
extern void	tienet_master_begin();
extern void	tienet_master_end();
extern void	tienet_master_wait();
extern void	tienet_master_shutdown();
extern void	tienet_master_broadcast(const void* mesg, size_t mesg_len);
extern uint64_t	tienet_master_get_rays_fired();

extern int	tienet_master_active_slaves;
extern int	tienet_master_socket_num;
extern uint64_t	tienet_master_transfer;
extern int	tienet_master_verbose;

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
