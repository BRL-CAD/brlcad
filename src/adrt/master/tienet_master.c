/*                 T I E N E T _ M A S T E R . C
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
/** @file tienet_master.c
 *
 *  Comments -
 *      TIE Networking Master
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_PTHREAD_H
#  include <pthread.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif

#include "libtie/tie.h"
#include "adrt.h"
#include "tienet.h"
#include "tienet_master.h"

#include "bio.h"

#include "tie.h"

#include "bu.h"

#include "master.h"

#if TN_COMPRESSION
# include <zlib.h>
#endif



void tienet_sem_init(tienet_sem_t *sem, int val)
{
    pthread_mutex_init(&sem->mut, 0);
    pthread_cond_init(&sem->cond, 0);
    sem->val = val;
}

void tienet_sem_free(tienet_sem_t *sem)
{
    pthread_mutex_destroy(&sem->mut);
    pthread_cond_destroy(&sem->cond);
}

void tienet_sem_post(tienet_sem_t *sem)
{
    pthread_mutex_lock(&sem->mut);
    sem->val++;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mut);
}

void tienet_sem_wait(tienet_sem_t *sem)
{
    pthread_mutex_lock(&sem->mut);
    if (!sem->val)
        pthread_cond_wait(&sem->cond, &sem->mut);
    sem->val--;
    pthread_mutex_unlock(&sem->mut);
}




typedef struct tienet_master_data_s {
    void *data;
    int size;	/* Current size of work in bytes */
} tienet_master_data_t;


typedef struct tienet_master_socket_s {
    int active;	/* Once a slave has completed its first work unit this becomes 1 */
    int idle;	/* When a slave has finished a work unit and there's nothing left for it to work on */
    int num;
    tienet_master_data_t mesg;	/* Used for a broadcast message */
    tienet_master_data_t work;	/* The work unit currently being processed */
    struct tienet_master_socket_s *prev;
    struct tienet_master_socket_s *next;
} tienet_master_socket_t;


void				tienet_master_init(int port, void fcb_result(tienet_buffer_t *result), char *list, char *exec, int buffer_size, int ver_key, int verbose);
void				tienet_master_free(void);
void				tienet_master_push(const void *data, size_t size);
void				tienet_master_shutdown(void);
void				tienet_master_broadcast(const void *mesg, size_t mesg_len);

void				tienet_master_begin(void);
void				tienet_master_end(void);
void				tienet_master_wait(void);

void				tienet_master_connect_slaves(fd_set *readfds);
void*				tienet_master_listener(void *ptr);
void				tienet_master_send_work(tienet_master_socket_t *sock);
void				tienet_master_result(tienet_master_socket_t *sock);
void				tienet_master_shutdown(void);

static int			tienet_master_ver_key;
static int			tienet_master_port;
static int			tienet_master_highest_fd;

int				tienet_master_active_slaves;
int				tienet_master_socket_num;
static tienet_master_socket_t	*tienet_master_socket_list;
static tienet_master_socket_t	*tienet_master_dead_socket_list;

int				tienet_master_buffer_size;
static tienet_master_data_t	*tienet_master_buffer;		/* Buffer */
static int			tienet_master_pos_fill;
static int			tienet_master_pos_read;

static tienet_sem_t		tienet_master_sem_fill;	/* Fill Buffer Semaphore */
static tienet_sem_t		tienet_master_sem_read;	/* Read Buffer Semaphore */
static tienet_sem_t		tienet_master_sem_app;	/* Application Semaphore */
static tienet_sem_t		tienet_master_sem_out;	/* Outstanding Semaphore */
static tienet_sem_t		tienet_master_sem_shutdown; /* Shutdown Semaphore */

uint64_t			tienet_master_transfer;
static char			tienet_master_exec[64];	/* Something to run in order to jumpstart the slaves */
static char			tienet_master_list[64]; /* A list of slaves in daemon mode to connect to */
int				tienet_master_verbose;
int				tienet_master_endflag;
int				tienet_master_shutdown_state;
int				tienet_master_halt_networking;

tienet_buffer_t			tienet_master_result_buffer;
pthread_mutex_t			tienet_master_send_mut;
pthread_mutex_t			tienet_master_push_mut;
pthread_mutex_t			tienet_master_broadcast_mut;


#if TN_COMPRESSION
tienet_buffer_t			tienet_master_result_buffer_comp;
#endif

/* result data callback */
typedef void tienet_master_fcb_result_t(tienet_buffer_t *result);
tienet_master_fcb_result_t	*tienet_master_fcb_result;


void tienet_master_init(int port, void fcb_result(tienet_buffer_t *result), char *list, char *exec, int buffer_size, int ver_key, int verbose)
{
    pthread_t thread;
    int i;


    tienet_master_port = port;
    tienet_master_verbose = verbose;
    tienet_master_buffer_size = buffer_size;
    tienet_master_buffer = (tienet_master_data_t *)bu_malloc(sizeof(tienet_master_data_t) * tienet_master_buffer_size, "initial tienet buffer");

    tienet_master_fcb_result = fcb_result;
    tienet_master_active_slaves = 0;
    tienet_master_socket_num = 0;
    tienet_master_socket_list = NULL;
    tienet_master_dead_socket_list = NULL;

    tienet_master_transfer = 0;
    tienet_master_endflag = 0;
    tienet_master_shutdown_state = 0;
    tienet_master_halt_networking = 0;

    tienet_master_pos_fill = 0;
    tienet_master_pos_read = 0;

    TIENET_BUFFER_INIT(tienet_master_result_buffer);
#if TN_COMPRESSION
    TIENET_BUFFER_INIT(tienet_master_result_buffer_comp);
#endif

    strncpy(tienet_master_list, list, sizeof(tienet_master_list));
    strncpy(tienet_master_exec, exec, sizeof(tienet_master_exec));

    /* Copy version key to validate slaves of correct version are connecting */
    tienet_master_ver_key = ver_key;

    /* Force the first slave that connects to perform a Fill Buffer -> Read Buffer Move */
    tienet_sem_init(&tienet_master_sem_fill, 0);
    tienet_sem_init(&tienet_master_sem_read, 0);

    /* Allow the application to fill the buffer with tienet_master_buffer_size */
    for (i = 0; i < tienet_master_buffer_size; i++) {
	tienet_master_buffer[i].data = NULL;
	tienet_master_buffer[i].size = 0;
	tienet_sem_post(&tienet_master_sem_fill);
    }

    tienet_sem_init(&tienet_master_sem_app, 0);
    tienet_sem_init(&tienet_master_sem_out, 0);
    pthread_mutex_init(&tienet_master_send_mut, 0);
    pthread_mutex_init(&tienet_master_push_mut, 0);
    pthread_mutex_init(&tienet_master_broadcast_mut, 0);

    /* Start the Listener as a Thread */
    pthread_create(&thread, NULL, tienet_master_listener, NULL);
}


void tienet_master_free()
{
    int i;
    tienet_master_socket_t *sock;

    tienet_sem_free(&tienet_master_sem_fill);
    tienet_sem_free(&tienet_master_sem_read);
    tienet_sem_free(&tienet_master_sem_app);
    tienet_sem_free(&tienet_master_sem_out);

    TIENET_BUFFER_FREE(tienet_master_result_buffer);
#if TN_COMPRESSION
    TIENET_BUFFER_FREE(tienet_master_result_buffer_bomp);
#endif
    bu_free(tienet_master_buffer, "tienet master buffer");

    for (i = 0; i < tienet_master_buffer_size; i++)
	bu_free(tienet_master_buffer[i].data, "tienet master buffer data");

    for (sock = tienet_master_socket_list->next; sock; sock = sock->next)
	bu_free(sock->prev, "master socket");
}


void tienet_master_push(const void *data, size_t size)
{
    tienet_master_socket_t *socket, *tmp;
    short op;

    pthread_mutex_lock(&tienet_master_push_mut);

    op = TN_OP_SENDWORK;

    /* Decrement semaphore */
    tienet_sem_wait(&tienet_master_sem_fill);

    /* Fill buffer, Grow if necessary */
    if (sizeof(short) + sizeof(int) + size > tienet_master_buffer[tienet_master_pos_fill].size) {
	tienet_master_buffer[tienet_master_pos_fill].size = sizeof(short) + sizeof(int) + size;
	tienet_master_buffer[tienet_master_pos_fill].data = bu_realloc(tienet_master_buffer[tienet_master_pos_fill].data, tienet_master_buffer[tienet_master_pos_fill].size, "master buffer data");
    }
    TCOPY(short, &op, 0, tienet_master_buffer[tienet_master_pos_fill].data, 0);
    TCOPY(int, &size, 0, tienet_master_buffer[tienet_master_pos_fill].data, sizeof(short));
    memcpy(&((char *)(tienet_master_buffer[tienet_master_pos_fill].data))[sizeof(short) + sizeof(int)], data, size);

    /* Circular Increment */
    tienet_master_pos_fill = (tienet_master_pos_fill + 1) % tienet_master_buffer_size;
    tienet_sem_post(&tienet_master_sem_read);

    /* Process items in tienet_master_DeadSocketList */
    for (socket = tienet_master_dead_socket_list; socket;) {
	tienet_sem_wait(&tienet_master_sem_fill);
	TCOPY(int, &(socket->work.size), 0, &size, 0);

	/* Fill buffer, Grow if necessary */
	if (sizeof(short) + sizeof(int) + size > tienet_master_buffer[tienet_master_pos_fill].size) {
	    tienet_master_buffer[tienet_master_pos_fill].size = sizeof(short) + sizeof(int) + size;
	    tienet_master_buffer[tienet_master_pos_fill].data = bu_realloc(tienet_master_buffer[tienet_master_pos_fill].data, tienet_master_buffer[tienet_master_pos_fill].size, "master buffer data");
	}
	memcpy(tienet_master_buffer[tienet_master_pos_fill].data, socket->work.data, sizeof(short) + sizeof(int) + size);

	/* Circular Increment */
	tienet_master_pos_fill = (tienet_master_pos_fill + 1) % tienet_master_buffer_size;

	tmp = socket;
	if (socket == tienet_master_dead_socket_list)
	    tienet_master_dead_socket_list = tienet_master_dead_socket_list->next;
	socket = socket->next;

	bu_free(tmp->work.data, "work data");
	bu_free(tmp, "tmp socket");

	tienet_sem_post(&tienet_master_sem_read);
    }


    /*
     * Tell any idle slaves to get back to work.
     * This is the case where slaves have exhausted the work buffer,
     * then new work becomes available.
     */
    for (socket = tienet_master_socket_list; socket; socket = socket->next) {
	/* Only if not master socket do we send data to slave */
	if (socket->next && socket->idle)
	    tienet_master_send_work(socket);
    }

    pthread_mutex_unlock(&tienet_master_push_mut);
}


void tienet_master_begin()
{
    if (!tienet_master_sem_app.val)
	tienet_master_endflag = 0;
}


void tienet_master_end()
{
    tienet_master_endflag = 1;
}


void tienet_master_wait()
{
    tienet_sem_wait(&tienet_master_sem_app);
}


void tienet_master_connect_slaves(fd_set *readfds)
{
    FILE				*fh;
    struct	sockaddr_in	daemon, slave;
    struct	hostent		slave_ent;
    tienet_master_socket_t	*tmp;
    short				op;
    char				host[64], *temp;
    int				daemon_socket, port, slave_ver_key;


    fh = fopen(tienet_master_list, "rb");
    if (fh) {
	while (!feof(fh)) {
	    bu_fgets(host, 64, fh);
	    if (host[0]) {
		port = TN_SLAVE_PORT;
		temp = strchr(host, ':');
		if (temp) {
		    port = atoi(&temp[1]);
		    temp[0] = 0;
		} else {
		    host[strlen(host)-1] = 0;
		}

		/* check to see if this slave is in dns */
		if (gethostbyname(host)) {
		    slave_ent = gethostbyname(host)[0];

		    /* This is what we're connecting to */
		    slave.sin_family = slave_ent.h_addrtype;
		    memcpy((char *)&slave.sin_addr.s_addr, slave_ent.h_addr_list[0], slave_ent.h_length);
		    slave.sin_port = htons(port);

		    /* Make a communications socket that will get stuffed into the list */
		    daemon_socket = socket(AF_INET, SOCK_STREAM, 0);
		    if (daemon_socket < 0) {
			fprintf(stderr, "unable to create  socket, exiting.\n");
			exit(1);
		    }

		    daemon.sin_family = AF_INET;
		    daemon.sin_addr.s_addr = htonl(INADDR_ANY);
		    daemon.sin_port = htons(0);

		    if (bind(daemon_socket, (struct sockaddr *)&daemon, sizeof(daemon)) < 0) {
			fprintf(stderr, "unable to bind socket, exiting.\n");
			exit(1);
		    }

		    /* Make an attempt to connect to this host and initiate work */
		    if (connect(daemon_socket, (struct sockaddr *)&slave, sizeof(slave)) < 0) {
			fprintf(stderr, "cannot connect to slave: %s:%d, skipping.\n", host, port);
		    } else {
			/* Send endian to slave */
			op = 1;
			tienet_send(daemon_socket, &op, sizeof(short));

			/* Read Version Key and Compare to ver_key, if valid proceed, if not tell slave to disconnect */
			tienet_recv(daemon_socket, &slave_ver_key, sizeof(int));
			if (slave_ver_key != tienet_master_ver_key) {
			    op = TN_OP_COMPLETE;
			    tienet_send(daemon_socket, &op, sizeof(short));
			} else {
			    op = TN_OP_OKAY;
			    tienet_send(daemon_socket, &op, sizeof(short));

			    /* Append to select list */
			    tmp = tienet_master_socket_list;
			    tienet_master_socket_list = (tienet_master_socket_t *)bu_malloc(sizeof(tienet_master_socket_t), "socket list");
			    tienet_master_socket_list->next = tmp;
			    tienet_master_socket_list->prev = NULL;
			    tienet_master_socket_list->work.data = NULL;
			    tienet_master_socket_list->work.size = 0;
			    tienet_master_socket_list->mesg.data = NULL;
			    tienet_master_socket_list->mesg.size = 0;
			    tienet_master_socket_list->num = daemon_socket;
			    tienet_master_socket_list->active = 0;
			    tienet_master_socket_list->idle = 1;

			    tmp->prev = tienet_master_socket_list;
			    tienet_master_socket_num++;
			    FD_SET(daemon_socket, readfds);

			    /* Check to see if it's the new highest */
			    if (daemon_socket > tienet_master_highest_fd)
				tienet_master_highest_fd = daemon_socket;
			}
		    }
		} else {
		    fprintf(stderr, "unknown host: %s, skipping.\n", host);
		}
	    }
	    host[0] = 0;
	}
	fclose(fh);
    }
}


void* tienet_master_listener(void *ptr)
{
    struct	sockaddr_in	master, slave;
    socklen_t			addrlen;
    fd_set			readfds;
    tienet_master_socket_t	*sock, *tmp;
    int				r, master_socket, slave_socket, slave_ver_key;
    short				op;


    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
	fprintf(stderr, "cannot creating socket, exiting.\n");
	exit(1);
    }

    addrlen = sizeof(struct sockaddr_in);
    master.sin_family = AF_INET;
    master.sin_addr.s_addr = INADDR_ANY;
    master.sin_port = htons(tienet_master_port);

    if (bind(master_socket, (struct sockaddr *)&master, addrlen)) {
	fprintf(stderr, "socket already bound, exiting.\n");
	exit(1);
    }

    /* Set first socket as master, rest are slaves - LIFO Stack - Always gets processed last */
    tienet_master_socket_list = (tienet_master_socket_t *)bu_malloc(sizeof(tienet_master_socket_t), "socket list");
    tienet_master_socket_list->next = NULL;
    tienet_master_socket_list->prev = NULL;
    tienet_master_socket_list->work.data = NULL;
    tienet_master_socket_list->work.size = 0;
    tienet_master_socket_list->mesg.data = NULL;
    tienet_master_socket_list->mesg.size = 0;
    tienet_master_socket_list->num = master_socket;
    tienet_master_highest_fd = master_socket;

    /* Listen for connections */
    master_listener_result = listen(master_socket, 3);

    addrlen = sizeof(slave);

    FD_ZERO(&readfds);
    FD_SET(master_socket, &readfds);

    /* Execute script - used for spawning slaves */
    system(tienet_master_exec);

    /* Process slave host list - used for connecting to running daemons */
    tienet_master_connect_slaves(&readfds);

    /* Handle Network Communications */
    while (1) {
	select(tienet_master_highest_fd+1, &readfds, NULL, NULL, NULL);

	/*
	 * Kill this thread if the shutdown call has been made and work
	 * units that were out have come back in.
	 */
	if (tienet_master_halt_networking)
	    return NULL;


	/* Slave Communication */
	for (sock = tienet_master_socket_list; sock; sock = sock->next) {
	    if (FD_ISSET(sock->num, &readfds)) {
		if (sock->num == master_socket) {
		    /* New Connections, Always LAST in readfds list */
		    slave_socket = accept(master_socket, (struct sockaddr *)&slave, &addrlen);
		    if (slave_socket >= 0) {
			if (tienet_master_verbose)
			    printf ("The slave %s has connected on port: %d, sock_num: %d\n", inet_ntoa(slave.sin_addr), tienet_master_port, slave_socket);
			tmp = tienet_master_socket_list;
			tienet_master_socket_list = (tienet_master_socket_t *)bu_malloc(sizeof(tienet_master_socket_t), "master socket list");
			tienet_master_socket_list->next = tmp;
			tienet_master_socket_list->prev = NULL;
			tienet_master_socket_list->work.data = NULL;
			tienet_master_socket_list->work.size = 0;
			tienet_master_socket_list->mesg.data = NULL;
			tienet_master_socket_list->mesg.size = 0;
			tienet_master_socket_list->num = slave_socket;
			tienet_master_socket_list->active = 0;
			tienet_master_socket_list->idle = 1;
			tmp->prev = tienet_master_socket_list;
			tienet_master_socket_num++;

			/* Send endian to slave */
			op = 1;
			tienet_send(slave_socket, &op, sizeof(short));
			/* Read Version Key and Compare to ver_key, if valid proceed, if not tell slave to disconnect */
			tienet_recv(slave_socket, &slave_ver_key, sizeof(int));
			if (slave_ver_key != tienet_master_ver_key) {
			    op = TN_OP_COMPLETE;
			    tienet_send(slave_socket, &op, sizeof(short));
			} else {
			    /* Version is okay, proceed */
			    op = TN_OP_OKAY;
			    tienet_send(slave_socket, &op, sizeof(short));

			    /* Append to select list */
			    if (slave_socket > tienet_master_highest_fd)
				tienet_master_highest_fd = slave_socket;
			}

			/* Send some work */
/*            tienet_master_send_work(sock); */
		    }
		} else {
		    /* Make sure socket is still active on this recv */
		    r = tienet_recv(sock->num, &op, sizeof(short));
		    /* if "r", error code returned, remove slave from pool */
		    if (r) {
			tienet_master_socket_t		*tmp2;
			/* Because master socket is always last there is no need to check if "next" exists.
			 * Remove this socket from chain and link prev and next up to fill gap. */
			if (sock->prev)
			    sock->prev->next = sock->next;
			sock->next->prev = sock->prev;

			/* Store ptr to sock before we modify it */
			tmp = sock;
			/* If the socket is the head then we need to not only modify the socket,
			 * but the head as well. */
			if (tienet_master_socket_list == sock)
			    tienet_master_socket_list = sock->next;
			sock = sock->prev ? sock->prev : sock->next;

			/* Put the socket into the tienet_master_DeadSocketList */
			if (tienet_master_dead_socket_list) {
			    tmp2 = tienet_master_dead_socket_list;
			    tienet_master_dead_socket_list = tmp;
			    tienet_master_dead_socket_list->prev = NULL;
			    tienet_master_dead_socket_list->next = tmp2;
			    tmp2->prev = tienet_master_dead_socket_list;
			} else {
			    tienet_master_dead_socket_list = tmp;
			    tienet_master_dead_socket_list->next = NULL;
			    tienet_master_dead_socket_list->prev = NULL;
			}

			tienet_master_socket_num--;
		    } else {
			/*
			 * Slave Op Instructions
			 */
			switch (op) {
			    case TN_OP_REQWORK:
				tienet_master_send_work(sock);
				break;

			    case TN_OP_RESULT:
				tienet_master_result(sock);
				break;

			    default:
				break;
			}
		    }
		}
	    }
	}

	/* Rebuild select list for next select call */
	tienet_master_highest_fd = 0;
	for (sock = tienet_master_socket_list; sock; sock = sock->next) {
	    if (sock->num > tienet_master_highest_fd)
		tienet_master_highest_fd = sock->num;
	    FD_SET(sock->num, &readfds);
	}
    }
}


void tienet_master_send_work(tienet_master_socket_t *sock)
{
    int size;
    short op;

    /*
     * This exists to prevent a collision from tienet_master_push calling this function
     * as a result of a socket being idle and then given work.  If this function were called
     * and 2 threads entered the if (tienet_master_sem_read.val) block and waited on tienet_master_sem_read
     * with the first one hitting the wait on a value of one the other one could end up waiting forever.
     */
    pthread_mutex_lock(&tienet_master_send_mut);

    /*
     * If shutdown has been initiated, do not send any data to the slaves,
     * instead they need to wait for a shutdown message.
     */
    if (tienet_master_shutdown_state) {
	/* if no work units are out, allow the shutdown to proceed. */
	if (!tienet_master_sem_out.val)
	    tienet_sem_post(&tienet_master_sem_shutdown);
	return;
    }


    /*
     * Check to see if a broadcast message is sitting in the queue.
     * The mutex prevents a read and write from occuring at the same time.
     */
    pthread_mutex_lock(&tienet_master_broadcast_mut);
    if (sock->mesg.size) {
	op = TN_OP_SENDWORK;
	tienet_send(sock->num, &op, sizeof(short));
	tienet_send(sock->num, &sock->mesg.size, sizeof(int));
	tienet_send(sock->num, sock->mesg.data, sock->mesg.size);

	bu_free(sock->mesg.data, "message data");
	sock->mesg.data = NULL;
	sock->mesg.size = 0;

	pthread_mutex_unlock(&tienet_master_broadcast_mut);
	pthread_mutex_unlock(&tienet_master_send_mut);
	return;
    }
    pthread_mutex_unlock(&tienet_master_broadcast_mut);


    /* Check to see if work is available */
    if (tienet_master_sem_read.val) {
	sock->idle = 0;
	tienet_sem_wait(&tienet_master_sem_read);

	/* Increment counter for work units out */
	tienet_sem_post(&tienet_master_sem_out);

	/* Send Work Unit */
	TCOPY(int, tienet_master_buffer[tienet_master_pos_read].data, sizeof(short), &size, 0);
	tienet_send(sock->num, tienet_master_buffer[tienet_master_pos_read].data, sizeof(short) + sizeof(int) + size);

	if (sizeof(short) + sizeof(int) + size > sock->work.size) {
	    sock->work.size = sizeof(short) + sizeof(int) + size;
	    sock->work.data = bu_realloc(sock->work.data, sock->work.size, "work data");
	}

	/* Make a copy of this data in the slave list */
	memcpy(sock->work.data, tienet_master_buffer[tienet_master_pos_read].data, sizeof(short) + sizeof(int) + size);

	/* Circular Increment */
	tienet_master_pos_read = (tienet_master_pos_read + 1) % tienet_master_buffer_size;

	/* Application is free to push another work unit into the buffer */
	tienet_sem_post(&tienet_master_sem_fill);

	tienet_master_transfer += size;
    } else {
	/* no work was available, socket is entering an active idle state. */
	sock->idle = 1;
    }

    pthread_mutex_unlock(&tienet_master_send_mut);
}


void tienet_master_result(tienet_master_socket_t *sock)
{
#if TN_COMPRESSION
    unsigned long	dest_len;
#endif

    /* A work unit has come in, this slave is officially active */
    if (!sock->active) {
	sock->active = 1;
	tienet_master_active_slaves++;
    }

    /* Decrement counter for work units out */
    tienet_sem_wait(&tienet_master_sem_out);

    /* receive result length */
    tienet_recv(sock->num, &tienet_master_result_buffer.ind, sizeof(unsigned int));
    tienet_master_transfer += sizeof(unsigned int);

    /* allocate memory for result buffer if more is needed */
    TIENET_BUFFER_SIZE(tienet_master_result_buffer, tienet_master_result_buffer.ind);
#if TN_COMPRESSION
    /* receive compressed length */
    tienet_recv(sock->num, &tienet_master_result_buffer_comp.ind, sizeof(unsigned int));

    TIENET_BUFFER_SIZE(tienet_master_result_buffer_comp, comp_len);

    tienet_recv(sock->num, tienet_master_result_buffer_comp.data, comp_len);

    /* uncompress the data */
    dest_len = tienet_master_result_buffer.ind+32;	/* some extra padding for zlib to work with */
    uncompress(tienet_master_result_buffer.data, &dest_len, tienet_master_result_buffer_comp.data, tienet_master_result_buffer_comp.ind);

    tienet_master_transfer += tienet_master_result_buffer_comp.ind + sizeof(unsigned int);
#else
    /* receive result data */
    tienet_recv(sock->num, tienet_master_result_buffer.data, tienet_master_result_buffer.ind);
    tienet_master_transfer += tienet_master_result_buffer.ind;
#endif

    /* Send next work unit out before processing results so that slave is not waiting while result is being processed. */
    tienet_master_send_work(sock);

    /* Application level result callback function to process results. */
    tienet_master_fcb_result(&tienet_master_result_buffer);

    /*
     * If there's no units still out, the application has indicated it's done generating work,
     * and there's no available work left in the buffer, we're done.
     */

    if (!tienet_master_sem_out.val && tienet_master_endflag && !tienet_master_sem_read.val) {
	/* Release the wait semaphore, we're all done. */
	tienet_sem_post(&tienet_master_sem_shutdown);
	tienet_sem_post(&tienet_master_sem_app);
    }
}


void tienet_master_shutdown()
{
    short op;
    tienet_master_socket_t *socket;


    tienet_master_shutdown_state = 1;
    printf("Master is shutting down, standby.\n");
    tienet_sem_wait(&tienet_master_sem_shutdown);
    tienet_master_halt_networking = 1;

    /* Close Sockets */
    for (socket = tienet_master_socket_list; socket; socket = socket->next) {
	if (socket->next) {
	    /* Only if slave socket do we send data to it. */
	    op = TN_OP_COMPLETE;
	    tienet_send(socket->num, &op, sizeof(short));

	    /*
	     * Wait on Recv.  When slave socket closes, select will be triggered.
	     * At this point we know for sure the slave has disconnected.  This prevents
	     * the master socket from being closed before the slave socket, thus pushing
	     * the socket into an evil wait state
	     */

	    tienet_recv(socket->num, &op, sizeof(short));
	    close(socket->num);
	}
    }

    printf("Total data transfered: %.1f MiB\n", (tfloat)tienet_master_transfer/(tfloat)(1024*1024));
}


/* This function does not support message queuing right now, so don't try it. */
void tienet_master_broadcast(const void *mesg, size_t mesg_len)
{
    tienet_master_socket_t *socket;

    /* Prevent a Read and Write of the broadcast from occuring at the same time */
    pthread_mutex_lock(&tienet_master_broadcast_mut);

    /* Send a message to each available socket */
    for (socket = tienet_master_socket_list; socket; socket = socket->next) {
	/* Only if not master socket */
	if (socket->next) {
	    socket->mesg.size = mesg_len;
	    socket->mesg.data = bu_malloc(mesg_len, "message data");
	    memcpy(socket->mesg.data, mesg, mesg_len);
	}
    }
    pthread_mutex_unlock(&tienet_master_broadcast_mut);

    /*
     * Tell any idle slaves to get back to work.
     * This is the case where slaves have exhausted the work buffer,
     * then new work becomes available.
     */
    for (socket = tienet_master_socket_list; socket; socket = socket->next) {
	/* Only if not master socket do we send data to slave */
	if (socket->next && socket->idle)
	    tienet_master_send_work(socket);
    }
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
