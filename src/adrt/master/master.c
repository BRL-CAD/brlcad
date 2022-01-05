/*                        M A S T E R . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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
/** @file master.c
 *
 */

#include "common.h"

/* interface header */
#include "./master.h"

/* system headers */
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_GETOPT_H
#  include <getopt.h>
#endif
#include "zlib.h"
#include "bnetwork.h"
#include "bio.h"

/* public api headers */
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/debug.h"
#include "bu/getopt.h"
#include "bu/str.h"
#include "bu/snooze.h"

/* adrt headers */
#include "adrt.h"		/* adrt Defines */
#include "adrt_struct.h"	/* adrt common structs */
#include "tienet.h"
#include "tienet_master.h"
#include "camera.h"
#include "dispatcher.h"		/* Dispatcher that creates work units */
#include "compnet.h"		/* Component Networking, Sends Component Names via Network */


/* socket structure */
typedef struct master_socket_s
{
    int32_t num;
    int32_t controller;
    int32_t active;
    tienet_sem_t frame_sem;
    struct master_socket_s *prev;
    struct master_socket_s *next;
} master_socket_t;


typedef struct master_s
{
    uint8_t wid_list[ADRT_MAX_WORKSPACE_NUM];

    uint16_t image_w;
    uint16_t image_h;
    uint16_t image_format;

    uint32_t tile_num;

    master_socket_t *socklist;
    tienet_sem_t wait_sem;

    tienet_buffer_t buf;
    tienet_buffer_t buf_comp;

    uint32_t frame_ind;
    uint8_t slave_data[64];
    uint32_t slave_data_len;
    bu_thrd_t networking_thread;
    uint32_t active_connections;
    uint32_t alive;
} master_t;


void master_dispatcher_init();
int master_networking(void *ptr);
void master_result(tienet_buffer_t *result);

master_t master;

static void
master_setup()
{
    uint16_t i;

    master.active_connections = 0;

    master.alive = 1;

    master.frame_ind = 0;

    TIENET_BUFFER_INIT(master.buf);
    TIENET_BUFFER_INIT(master.buf_comp);

    /* -1 indicates this slot is not used and thus does not contain an open project. */
    for (i = 0; i < ADRT_MAX_WORKSPACE_NUM; i++)
	master.wid_list[i] = 0;
}


void
master_init(int port, int obs_port, char *list, char *exec, char *comp_host, int verbose)
{
    /* Setup defaults */
    master_setup();

    /* Initialize tienet master */
    master.tile_num = DISPATCHER_TILE_NUM * DISPATCHER_TILE_NUM;
    tienet_master_init(port, master_result, list, exec, 5, ADRT_VER_KEY, verbose);

    /* Launch a thread to handle networking */
    bu_thrd_create(&master.networking_thread, (bu_thrd_start_t)master_networking, &obs_port);

    /* Connect to the component Server */
    compnet_connect(comp_host, ISST_COMPNET_PORT);

    /* Initialize the work dispatcher */
    master_dispatcher_init();

    tienet_sem_init(&master.wait_sem, 0);

    /* Process work units */
    tienet_sem_wait(&master.wait_sem);

    /* Wait for the tienet master work buffer to empty and for all the results to come back */
    tienet_master_wait();

    /* Shutdown */
    tienet_master_shutdown();

    /* Free network data */
    tienet_master_free();

    /* Free the dispatcher data */
    master_dispatcher_free();

    TIENET_BUFFER_FREE(master.buf);
    TIENET_BUFFER_FREE(master.buf_comp);

    /* Wait for networking thread to end */
    bu_thrd_join(master.networking_thread, NULL);
}


void
master_result(tienet_buffer_t *result)
{
    master_socket_t *sock;
    camera_tile_t tile;
    uint8_t *rgb_data, op;
    uint16_t wid;
    uint32_t i, ind, ind2, update;

    static int lastop;

    update = 0;
    ind = 0;

    op = result->data[ind];
    ind += 1;

    TCOPY(uint16_t, result->data, ind, &wid, 0);
    ind += 2;

    if (bu_debug & BU_DEBUG_UNUSED_2 && lastop != op) {
	bu_log("ADRT Master OP: %d\n", op);
	lastop = op;
    }

    switch (op) {
	case ADRT_WORK_FRAME:
	    /* Work unit data */
	    TCOPY(camera_tile_t, result->data, ind, &tile, 0);
	    ind += sizeof(camera_tile_t);

	    /* Pointer to RGB Data */
	    rgb_data = &result->data[ind];

	    /* Copy the tile into the image */
	    ind = 0;
	    ind2 = tile.orig_x + tile.orig_y * master.image_w;

	    /* Only does 24-bit right now */
	    for (i = 0; i < tile.size_y; i++) {
		memcpy(&master.buf.data[3*ind2], &rgb_data[3*ind], 3*tile.size_x);
		ind += tile.size_x;
		ind2 += master.image_w;
	    }

	    master.frame_ind++;

	    /* Image is complete, draw the frame. */
	    if (master.frame_ind == master.tile_num) {
		update = 1;
		master.frame_ind = 0;
		master.buf.ind = 3 * master.image_w * master.image_h;
	    }
	    break;

	case ADRT_WORK_SHOTLINE:
	    {
		tienet_buffer_t selection_buf;
		uint32_t num, tind;
		uint8_t c;
		char name[256];

		TIENET_BUFFER_INIT(selection_buf);
		TIENET_BUFFER_SIZE(selection_buf, result->ind);

		/* Make sure we have somewhere to copy data to */
		if (!selection_buf.data)
		    return;

		/* Send this data to the slaves as ADRT_WORK_SELECT for highlighting hit components */
		selection_buf.ind = 0;

		op = ADRT_WORK_SELECT;
		TCOPY(uint8_t, &op, 0, selection_buf.data, selection_buf.ind);
		selection_buf.ind += 1;

		TCOPY(uint16_t, &wid, 0, selection_buf.data, selection_buf.ind);
		selection_buf.ind += 2;

		tind = ind;

		/* Skip over in-hit */
		tind += sizeof(TIE_3);

		/* Number of meshes */
		TCOPY(uint32_t, result->data, tind, &num, 0);
		tind += 4;

		/* Reset Flag */
		op = 1;
		TCOPY(uint8_t, &op, 0, selection_buf.data, selection_buf.ind);
		selection_buf.ind += 1;

		/* Number of meshes */
		TCOPY(uint32_t, &num, 0, selection_buf.data, selection_buf.ind);
		selection_buf.ind += 4;

		/* For each intersected mesh extract the name and pack it, skipping the thickness */
		for (i = 0; i < num; i++) {
		    /* length of string */
		    TCOPY(uint8_t, result->data, tind, &c, 0);
		    tind += 1;

		    /* the name */
		    memcpy(name, &result->data[tind], c);
		    tind += c;

		    /* skip over the thickness */
		    tind += sizeof(TFLOAT);

		    /* pack the mesh name length and name */
		    TCOPY(uint8_t, &c, 0, selection_buf.data, selection_buf.ind);
		    selection_buf.ind += 1;

		    memcpy(&selection_buf.data[selection_buf.ind], name, c);
		    selection_buf.ind += c;
		}

		/* Shotline Selection data being sent to slaves */
		tienet_master_broadcast(selection_buf.data, selection_buf.ind);
		update = 1;

		TIENET_BUFFER_FREE(selection_buf);

		/* The data that will be sent to the observer */
		TIENET_BUFFER_SIZE(master.buf, result->ind - ind + 1);
		master.buf.ind = 0;
		memcpy(&master.buf.data[master.buf.ind], &result->data[ind], result->ind - ind);
		master.buf.ind += result->ind - ind;
	    }
	    break;

	case ADRT_WORK_MINMAX:
	    /* The data that will be sent to the observer */
	    TIENET_BUFFER_SIZE(master.buf, result->ind - ind);
	    master.buf.ind = 0;
	    memcpy(&master.buf.data[master.buf.ind], &result->data[ind], result->ind - ind);
	    master.buf.ind += result->ind - ind;
	    update = 1;
	    break;

	default:
	    break;
    }

    /* Alert the observers that a new frame is available for viewing */
    if (update)
	for (sock = master.socklist; sock; sock = sock->next)
	    if (sock->next)
		if (!sock->frame_sem.val)
		    tienet_sem_post(&(sock->frame_sem));
}


int
master_networking(void *ptr)
{
    master_socket_t *sock = NULL, *tmp = NULL;
    struct sockaddr_in master_addr = {0};
    struct sockaddr_in observer_addr = {0};
    fd_set readfds;
    int port=0, master_socket=0, highest_fd=0, new_socket=0, error=0;
    unsigned int addrlen = 0;
    uint8_t op = 0;
    uint16_t endian = 0;


    port = *(int *) ptr;


    /* create a socket */
    if ((master_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	fprintf(stderr, "unable to create socket, exiting.\n");
	exit(1);
    }

    /* initialize socket list */
    BU_ALLOC(master.socklist, master_socket_t);
    master.socklist->next = NULL;
    master.socklist->prev = NULL;
    master.socklist->num = master_socket;

    highest_fd = master_socket;


    /* server address */
    master_addr.sin_family = AF_INET;
    master_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    master_addr.sin_port = htons(port);

    /* bind socket */
    if (bind(master_socket, (struct sockaddr *)&master_addr, sizeof(master_addr)) < 0) {
	fprintf(stderr, "observer socket already bound, exiting.\n");
	exit(1);
    }

    FD_ZERO(&readfds);
    FD_SET(master_socket, &readfds);

    /* listen for connections */
    observer_listener_result = listen(master.socklist->num, 3);

    if (go_daemon_mode) {
	/* spinlock until other socket is good */
	while (master_listener_result == 1)
	    bu_snooze(BU_SEC2USEC(0));
	/* if both sockets are listening, background. */
	if (master_listener_result == 0 && observer_listener_result == 0) {
#ifdef HAVE_WORKING_DAEMON_FUNCTION
	    daemon(0, 0);
#else
	    switch(fork()) {
	    case 0:
		/* child lives */
		break;
	    case -1:
		perror("fork failed");
		bu_exit(1, NULL);
		break;
	    default:
		/* kill parent */
		bu_exit(0, NULL);
	    }
#endif
	}
    }

    addrlen = sizeof(observer_addr);
    master.active_connections = 0;

    if (tienet_master_verbose)
	printf("Now listening\n");

    while (master.alive || master.active_connections) {
	/* wait for some network activity */
	select(highest_fd+1, &readfds, NULL, NULL, NULL);

	/* cycle through each socket and address the activity */
	for (sock = master.socklist; sock; sock = sock->next) {
	    /* if no longer alive then mark each of the active connections as no longer active */
	    if (!master.alive)
		sock->active = 0;

	    /* check if activity was on this socket */
	    if (!FD_ISSET(sock->num, &readfds))
		continue;

	    /* new connection */
	    if (sock->num == master_socket) {
		new_socket = accept(master_socket, (struct sockaddr *)&observer_addr, &addrlen);
		if (new_socket >= 0) {
		    tmp = master.socklist;

		    BU_ALLOC(master.socklist, master_socket_t);
		    master.socklist->num = new_socket;
		    master.socklist->controller = master.active_connections ? 0 : 1;
		    master.socklist->active = 1;
		    master.socklist->next = tmp;
		    master.socklist->prev = NULL;
		    tienet_sem_init(&(master.socklist->frame_sem), 0);
		    tmp->prev = master.socklist;
		    V_MAX(highest_fd, new_socket);
		    master.active_connections++;
		}
		continue;
	    }

	    /* remove socket from pool if there's an error, i.e. slave disconnected */
	    op = 255;
	    error = tienet_recv(sock->num, &op, 1);
	    if (error || op == ADRT_NETOP_QUIT || !sock->active) {
		op = ADRT_NETOP_QUIT;
		tienet_send(sock->num, &op, 1);

		tmp = sock;
		if (sock->prev)
		    sock->prev->next = sock->next;
		if (sock->next)
		    sock->next->prev = sock->prev;
		if (sock == master.socklist)
		    master.socklist = master.socklist->next;
		close(sock->num);
		if (!sock->next)
		    break;
		sock = sock->next;
		bu_free(tmp, "tmp socket");
		master.active_connections--;
		continue;
	    }

	    /* standard communication */
	    switch (op) {
		case ADRT_NETOP_SHUTDOWN:
		    tienet_sem_post(&master.wait_sem);
		    return 0;
		    break;

		case ADRT_NETOP_INIT:
		    /* send endian */
		    endian = 1;
		    tienet_send(sock->num, &endian, 2);
		    break;

		case ADRT_NETOP_REQWID:
		    {
			uint16_t i;

			/*
			 * Allocate a Workspace ID that the client application can
			 * use to pass along to the slaves to associate a project id with.
			 */
			i = 0;
			while (i < ADRT_MAX_WORKSPACE_NUM && master.wid_list[i])
			    i++;

			/* Mark this ID as being in use. */
			master.wid_list[i] = 1;

			/* Send this WID to the client application. */
			tienet_send(sock->num, &i, 2);
		    }
		    break;

		case ADRT_NETOP_LOAD:
		    {
			uint32_t size;
			void *mesg;

			tienet_recv(sock->num, &size, 4);
			mesg = bu_malloc(size, "message buffer");
			tienet_recv(sock->num, mesg, size);
			tienet_master_broadcast(mesg, size);
			bu_free(mesg, "message");
		    }
		    break;

		case ADRT_NETOP_WORK:
		    {
			uint16_t wid;

			/* Size */
			tienet_recv(sock->num, &master.slave_data_len, 4);
			tienet_recv(sock->num, master.slave_data, master.slave_data_len);

			op = master.slave_data[0];
			memcpy(&wid, &master.slave_data[1], 2);

			switch (op) {
			    case ADRT_WORK_FRAME_ATTR:
				TCOPY(uint16_t, master.slave_data, 3, &master.image_w, 0);
				TCOPY(uint16_t, master.slave_data, 5, &master.image_h, 0);
				TCOPY(uint16_t, master.slave_data, 7, &master.image_format, 0);
				TIENET_BUFFER_SIZE(master.buf, 3 * master.image_w * master.image_h);
				tienet_master_broadcast(master.slave_data, master.slave_data_len);
				tienet_sem_post(&(sock->frame_sem));
				break;

			    case ADRT_WORK_FRAME:
				{
				    /* Fill the work buffer */
				    master_dispatcher_generate(master.slave_data, master.slave_data_len, master.image_w, master.image_h, master.image_format);
				}
				break;

			    case ADRT_WORK_SHOTLINE:
				tienet_master_push(master.slave_data, master.slave_data_len);
				break;

			    case ADRT_WORK_SELECT:
				tienet_master_broadcast(master.slave_data, master.slave_data_len);
				tienet_sem_post(&(sock->frame_sem));
				break;

			    case ADRT_WORK_STATUS:
				tienet_master_broadcast(master.slave_data, master.slave_data_len);
				break;

			    case ADRT_WORK_MINMAX:
				tienet_master_push(master.slave_data, master.slave_data_len);
				break;

			    default:
				break;
			}

			/* Wait for the result to come back */
			tienet_sem_wait(&(sock->frame_sem));

			/* Stamp the result with the work type */
			tienet_send(sock->num, &op, 1);

			/* Workspace ID */
			tienet_send(sock->num, &wid, 2);

			/* Size of result data */
			tienet_send(sock->num, &master.buf.ind, 4);

#if defined(ADRT_USE_COMPRESSION) && ADRT_USE_COMPRESSION
			{
			    unsigned long dest_len;
			    unsigned int comp_size;

			    dest_len = master.buf.ind + 32;
			    TIENET_BUFFER_SIZE(master.buf_comp, dest_len);

			    /* result data */
			    compress(&master.buf_comp.data[sizeof(unsigned int)], &dest_len, master.buf.data, master.buf.ind);
			    comp_size = dest_len;

			    TCOPY(uint32_t, &comp_size, 0, master.buf_comp.data, 0);

			    /* int compressed data size in bytes followed by actual rgb frame data */
			    tienet_send(sock->num, master.buf_comp.data, comp_size + sizeof(unsigned int));
			}
#else
			/* result data */
			tienet_send(sock->num, master.buf.data, master.buf.ind);
#endif
		    }
		    break;

		case ADRT_NETOP_QUIT:
		    master.active_connections = 0;
		    break;

		default:
		    break;
	    }
	}

	/* Rebuild select list for next select call */
	highest_fd = 0;
	for (sock = master.socklist; sock; sock = sock->next) {
	    V_MAX(highest_fd, sock->num);
	    FD_SET(sock->num, &readfds);
	}
    }

    /* free master.socklist */
    if (master.socklist) {
	for (sock = master.socklist->next; sock; sock = sock->next)
	    bu_free(sock->prev, "master socket list");
    }

    return 0;
}


#ifdef HAVE_GETOPT_LONG
static struct option longopts[] =
{
    { "exec",	required_argument,	NULL, 'e' },
    { "help",	no_argument,		NULL, 'h' },
    { "comp_host",	required_argument,	NULL, 'c' },
    { "daemon",	no_argument,		NULL, 'd' },
    { "obs_port",	required_argument,	NULL, 'o' },
    { "port",	required_argument,	NULL, 'p' },
    { "build",	no_argument,		NULL, 'b' },
    { "verbose",	no_argument,		NULL, 'v' },
    { "list",	required_argument,	NULL, 'l' },
};
#endif

static char shortopts[] = "bc:de:i:ho:p:vl:h?";

unsigned char go_daemon_mode = 0;
unsigned int master_listener_result = 1;
unsigned int observer_listener_result = 1;

static void finish(int sig) {
    printf("Collected signal %d, aborting!\n", sig);
    exit(EXIT_FAILURE);
}


static void help() {
    fprintf(stderr,"%s\n", "Usage: adrt_master [options]\n\
  -h\t\tdisplay help.\n\
  -c\t\tconnect to component server.\n\
  -d\t\tdaemon mode.\n\
  -e\t\tscript to execute that starts slaves.\n\
  -l\t\tfile containing list of slaves to use as compute nodes.\n\
  -o\t\tset observer port number.\n\
  -p\t\tset master port number.\n\
  -v\t\tverbose.\n\
  -b\t\tdisplay build.\n");
}


int main(int argc, char **argv) {
    int port = 0, obs_port = 0, c = 0;
    char exec[64], list[64], comp_host[64];
    int verbose = 0;

    bu_setprogname(argv[0]);

    signal(SIGINT, finish);

    /* Initialize strings */
    list[0] = 0;
    exec[0] = 0;
    comp_host[0] = 0;
    port = TN_MASTER_PORT;
    obs_port = ADRT_PORT;

    /* Parse command line options */

    while ((c =
#ifdef HAVE_GETOPT_LONG
	    getopt_long(argc, argv, shortopts, longopts, NULL)
#else
	    bu_getopt(argc, argv, shortopts)
#endif
	       )!= -1)
    {
	if (bu_optopt == '?') c='h';
	switch (c) {
	    case 'c':
		bu_strlcpy(comp_host, bu_optarg, 64);
		break;

	    case 'd':
		go_daemon_mode = 1;
		break;

	    case 'h':
		help();
		return EXIT_SUCCESS;

	    case 'o':
		obs_port = atoi(bu_optarg);
		break;

	    case 'p':
		port = atoi(bu_optarg);
		break;

	    case 'l':
		bu_strlcpy(list, bu_optarg, 64);
		break;

	    case 'e':
		bu_strlcpy(exec, bu_optarg, 64);
		break;

	    case 'b':
		printf("adrt_master build: %s %s\n", __DATE__, __TIME__);
		return EXIT_SUCCESS;
		break;

	    case 'v':
		verbose = 1;
		break;

	    default:
		help();
		return EXIT_FAILURE;
	}
    }

    master_init(port, obs_port, list, exec, comp_host, verbose);

    return EXIT_SUCCESS;
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
