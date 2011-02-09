/*                  T I E N E T _ S L A V E . C
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
/** @file tienet_slave.c
 *
 *  Comments -
 *      TIE Networking Slave
 *
 */

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include "bio.h"
#include "tie.h"
#include "adrt.h"
#include "tienet.h"

#include "tienet_slave.h"

#if TN_COMPRESSION
# include <zlib.h>
#endif


void	tienet_slave_worker(int port, char *host);

int	tienet_slave_ver_key;


/* work function callback */
typedef void tienet_slave_fcb_work_t(tienet_buffer_t *work, tienet_buffer_t *result);
tienet_slave_fcb_work_t	*tienet_slave_fcb_work;

/* free function callback */
typedef void tienet_slave_fcb_free_t();
tienet_slave_fcb_free_t	*tienet_slave_fcb_free;

void tienet_slave_init(int port, char *host,
                       void fcb_work(tienet_buffer_t *work, tienet_buffer_t *result),
                       void fcb_free(void),
                       int ver_key)
{
    tienet_slave_fcb_work = fcb_work;
    tienet_slave_fcb_free = fcb_free;


    /* Store version key for comparisson with version key on master */
    tienet_slave_ver_key = ver_key;

    /*
     * If host is specified, connect to master, do work, shutdown.
     */

    if (host[0]) {
	tienet_slave_worker(port, host);
    } else {
	fprintf(stdout, "missing hostname, exiting.\n");
	exit(1);
    }
}


void tienet_slave_free()
{
}


void tienet_slave_worker(int port, char *host) {
    tienet_buffer_t result, buffer;
    struct sockaddr_in master, slave;
    struct hostent h;
    short op;
    uint32_t size;
    int slave_socket;
#if TN_COMPRESSION
    tienet_buffer_t buffer_comp;
    unsigned long dest_len;
#endif


    /* Initialize res_buf to NULL for realloc'ing */
    TIENET_BUFFER_INIT(result);
    TIENET_BUFFER_INIT(buffer);
#if TN_COMPRESSION
    TIENET_BUFFER_INIT(buffer_comp);
#endif

    if (gethostbyname(host)) {
	h = gethostbyname(host)[0];
    } else {
	fprintf(stderr, "unknown host: %s\n", host);
	exit(1);
    }

    master.sin_family = h.h_addrtype;
    memcpy((char *)&master.sin_addr.s_addr, h.h_addr_list[0], h.h_length);
    master.sin_port = htons(port);

    /* Create a socket */
    if ((slave_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	fprintf(stderr, "unable to create socket, exiting.\n");
	exit(1);
    }

    /* Bind any available port number */
    slave.sin_family = AF_INET;
    slave.sin_addr.s_addr = htonl(INADDR_ANY);
    slave.sin_port = htons(0);

    if (bind(slave_socket, (struct sockaddr *)&slave, sizeof(slave)) < 0) {
	fprintf(stderr, "unable to bind socket, exiting\n");
	exit(1);
    }

    /* connect to master and request work */
    if (connect(slave_socket, (struct sockaddr *)&master, sizeof(master)) < 0) {
	fprintf(stderr, "cannot connect to master, exiting.\n");
	exit(1);
    }

    /* receive endian of master (going away) */
    {
	short tienet_endian;
	tienet_recv(slave_socket, &tienet_endian, sizeof(short));
    }

    /* send version key, master will respond whether to continue or not */
    tienet_send(slave_socket, &tienet_slave_ver_key, sizeof(int));

    /* If version mismatch then exit */
    tienet_recv(slave_socket, &op, sizeof(short));
    if (op == TN_OP_COMPLETE)
	return;

    /* Request Work Unit */
    /*
    op = TN_OP_REQWORK;
    tienet_send(slave_socket, &op, sizeof(short));
    */

    while (1) {
	tienet_recv(slave_socket, &op, sizeof(short));
	if (op == TN_OP_SHUTDOWN || op == TN_OP_COMPLETE) {
	    close(slave_socket);
	    exit(0);
	} else {
	    tienet_recv(slave_socket, &size, sizeof(uint32_t));
	    TIENET_BUFFER_SIZE(buffer, size);
	    tienet_recv(slave_socket, buffer.data, size);
	    buffer.ind = size;

	    /* Process work and Generate Results */
	    tienet_slave_fcb_work(&buffer, &result);

	    if (!result.ind)
		continue;

	    /* Send Result Back, length of: result + op_code + result_length + compression_length */
	    TIENET_BUFFER_SIZE(buffer, result.ind+sizeof(short)+sizeof(int)+sizeof(uint32_t));

	    buffer.ind = 0;

	    /* Pack Operation Code */
	    op = TN_OP_RESULT;
	    TCOPY(short, &op, 0, buffer.data, buffer.ind);
	    buffer.ind += sizeof(short);

	    /* Pack Result Length */
	    TCOPY(uint32_t, &result.ind, 0, buffer.data, buffer.ind);
	    buffer.ind += sizeof(uint32_t);

#if TN_COMPRESSION
	    /* Compress the result buffer */
	    TIENET_BUFFER_SIZE(buffer_comp, result.ind+32);

	    dest_len = buffer_comp.size+32;
	    compress(buffer_comp.data, &dest_len, result.data, result.ind);
	    size = (uint32_t)dest_len;

	    /* Pack Compressed Result Length */
	    TCOPY(uint32_t, &size, 0, buffer.data, buffer.ind);
	    buffer.ind += sizeof(uint32_t);

	    /* Pack Compressed Result Data */
	    memcpy(&((char *)buffer.data)[buffer.ind], buffer_comp.data, size);
	    buffer.ind += size;
#else
	    /* Pack Result Data */
	    memcpy(&((char *)buffer.data)[buffer.ind], result.data, result.ind);
	    buffer.ind += result.ind;
#endif
	    tienet_send(slave_socket, buffer.data, buffer.ind);
	}
    }

    TIENET_BUFFER_FREE(result);
    TIENET_BUFFER_FREE(buffer);
#if TN_COMPRESSION
    TIENET_BUFFER_FREE(buffer_comp);
#endif
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
