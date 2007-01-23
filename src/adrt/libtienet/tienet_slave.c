/*                  T I E N E T _ S L A V E . C
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
/** @file tienet_slave.c
 *
 *  Comments -
 *      TIE Networking Slave
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

#include "tienet_slave.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include "tienet_define.h"
#include "tienet_util.h"
#if TN_COMPRESSION
# include <zlib.h>
#endif


int	tienet_slave_prep(int slave_socket, tie_t *tie);
void	tienet_slave_worker(int port, char *host);
void	tienet_slave_daemon(int port);

short	tienet_endian;
int	tienet_slave_ver_key;


/* init function callback */
typedef void tienet_slave_fcb_init_t(tie_t *tie, int socknum);
tienet_slave_fcb_init_t	*tienet_slave_fcb_init;

/* work function callback */
typedef void tienet_slave_fcb_work_t(tie_t *tie, void *data, unsigned int size, void **res_buf, unsigned int *res_len);
tienet_slave_fcb_work_t	*tienet_slave_fcb_work;

/* free function callback */
typedef void tienet_slave_fcb_free_t();
tienet_slave_fcb_free_t	*tienet_slave_fcb_free;

/* mesg function callback */
typedef void tienet_slave_fcb_mesg_t(void *mesg, unsigned int mesg_len);
tienet_slave_fcb_mesg_t	*tienet_slave_fcb_mesg;


void tienet_slave_init(int port, char *host,
                       void fcb_init(tie_t *tie, int socknum),
                       void fcb_work(tie_t *tie, void *data, unsigned int size, void **res_buf, unsigned int *res_len),
                       void fcb_free(void),
                       void fcb_mesg(void *mesg, unsigned int mesg_len),
                       int ver_key)
{
  tienet_slave_fcb_init = fcb_init;
  tienet_slave_fcb_work = fcb_work;
  tienet_slave_fcb_free = fcb_free;
  tienet_slave_fcb_mesg = fcb_mesg;


  /* Store version key for comparisson with version key on master */
  tienet_slave_ver_key = ver_key;

  /*
  * If host is specified, connect to master, do work, shutdown.
  * If host is not specified, run as daemon, listen for master to connect
  */

  if(host[0]) {
    tienet_slave_worker(port, host);
  } else {
    tienet_slave_daemon(port);
  }
}


void tienet_slave_free() {
}


void tienet_slave_worker(int port, char *host) {
  struct sockaddr_in	master, slave;
  struct hostent	h;
  short			op;
  unsigned int		size, res_len, data_max, ind;
  int			slave_socket;
  uint64_t		rays_fired;
  void			*data, *res_buf;
  tie_t			tie;
#if TN_COMPRESSION
  int			comp_max;
  void			*comp_buf;
  unsigned long		dest_len;
#endif


  /* Initialize res_buf to NULL for realloc'ing */
  res_buf = NULL;

  if(gethostbyname(host)) {
    h = gethostbyname(host)[0];
  } else {
    fprintf(stderr, "unknown host: %s\n", host);
    exit(1);
  }

  master.sin_family = h.h_addrtype;
  memcpy((char *)&master.sin_addr.s_addr, h.h_addr_list[0], h.h_length);
  master.sin_port = htons(port);

  /* Create a socket */
  if((slave_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "unable to create socket, exiting.\n");
    exit(1);
  }

  /* Bind any available port number */
  slave.sin_family = AF_INET;
  slave.sin_addr.s_addr = htonl(INADDR_ANY);
  slave.sin_port = htons(0);

  if(bind(slave_socket, (struct sockaddr *)&slave, sizeof(slave)) < 0) {
    fprintf(stderr, "unable to bind socket, exiting\n");
    exit(1);
  }

  /* connect to master and request work */
  if(connect(slave_socket, (struct sockaddr *)&master, sizeof(master)) < 0) {
    fprintf(stderr, "cannot connect to master, exiting.\n");
    exit(1);
  }

  /* receive endian of master */
  tienet_recv(slave_socket, &tienet_endian, sizeof(short), 0);
  tienet_endian = tienet_endian == 1 ? 0 : 1;

  /* send version key, master will respond whether to continue or not */
  tienet_send(slave_socket, &tienet_slave_ver_key, sizeof(int), tienet_endian);

  /* If version mismatch then exit */
  tienet_recv(slave_socket, &op, sizeof(short), tienet_endian);
  if(op == TN_OP_COMPLETE)
    return;

  /* Request Work Unit */
  op = TN_OP_REQWORK;
  tienet_send(slave_socket, &op, sizeof(short), tienet_endian);

  data_max = 0;
  data = NULL;
#if TN_COMPRESSION
  comp_max = 0;
  comp_buf = NULL;
#endif


  while(1) {
    tienet_recv(slave_socket, &op, sizeof(short), tienet_endian);
    if(op == TN_OP_SHUTDOWN || op == TN_OP_COMPLETE) {
      close(slave_socket);
      exit(0);
    } else if(op == TN_OP_MESSAGE) {
      unsigned int mesg_len;
      void *mesg;

      /* send this message to the application */
      tienet_recv(slave_socket, &mesg_len, sizeof(unsigned int), tienet_endian);
      mesg = malloc(mesg_len);
      tienet_recv(slave_socket, mesg, mesg_len, 0);
      tienet_slave_fcb_mesg(mesg, mesg_len);
      free(mesg);
    } else if(op == TN_OP_PREP) {
      /* This can be done better */

      /* Clean and Reinitialize TIE */
      tienet_slave_fcb_free();

      if(tienet_slave_prep(slave_socket, &tie)) {
        fprintf(stderr, "version mismatch, exiting.\n");
        close(slave_socket);
        exit(0);
      }

      /* Now that we've been updated, Request a Work Unit */
      op = TN_OP_REQWORK;
      tienet_send(slave_socket, &op, sizeof(short), tienet_endian);
    } else {
      tienet_recv(slave_socket, &size, sizeof(unsigned int), tienet_endian);
      if(size > data_max) {
        data_max = size;
        data = realloc(data, size);
      }

      tienet_recv(slave_socket, data, size, 0);
      /* Process work and Generate Results */
      tienet_slave_fcb_work(&tie, data, size, &res_buf, &res_len);

      /* Send Result Back, length of: result + op_code + rays_fired + result_length + compression_length */
      if(res_len+sizeof(short)+sizeof(uint64_t)+sizeof(int)+sizeof(unsigned int) > data_max) {
        data_max = res_len+sizeof(short)+sizeof(uint64_t)+sizeof(int)+sizeof(unsigned int);
        data = realloc(data, data_max);
      }

      ind = 0;
      /* Pack Operation Code */
      op = TN_OP_RESULT;
      if(tienet_endian) tienet_flip(&op, &op, sizeof(short));
      memcpy(&((char*)data)[ind], &op, sizeof(short));
      if(tienet_endian) tienet_flip(&op, &op, sizeof(short));
      ind += sizeof(short);

      /* Pack total number of rays fired */
      rays_fired = tie.rays_fired;
      if(tienet_endian) tienet_flip(&rays_fired, &rays_fired, sizeof(uint64_t));
      memcpy(&((char*)data)[ind], &rays_fired, sizeof(uint64_t));
      ind += sizeof(uint64_t);

      /* Pack Result Length */
      if(tienet_endian) tienet_flip(&res_len, &res_len, sizeof(unsigned int));
      memcpy(&((char*)data)[ind], &res_len, sizeof(unsigned int));
      if(tienet_endian) tienet_flip(&res_len, &res_len, sizeof(unsigned int));
      ind += sizeof(unsigned int);

#if TN_COMPRESSION
      /* Compress the result buffer */
      if(comp_max < res_len) {
        comp_buf = realloc(comp_buf, res_len+32);	/* 32 bytes of extra padding for zlib */
        comp_max = res_len;
      }

      dest_len = comp_max+32;
      compress(comp_buf, &dest_len, res_buf, res_len);
      size = (unsigned int)dest_len;

      /* Pack Compressed Result Length */
      if(tienet_endian) tienet_flip(&size, &size, sizeof(unsigned int));
      memcpy(&((char*)data)[ind], &size, sizeof(unsigned int));
      if(tienet_endian) tienet_flip(&size, &size, sizeof(unsigned int));
      ind += sizeof(unsigned int);

      /* Pack Compressed Result Data */
      memcpy(&((char*)data)[ind], comp_buf, size);
      ind += size;
#else
      /* Pack Result Data */
      memcpy(&((char*)data)[ind], res_buf, res_len);
      ind += res_len;
#endif
      tienet_send(slave_socket, data, ind, 0);
    }
  }

  free(data);
  free(res_buf);
#if TN_COMPRESSION
  free(comp_buf);
#endif
}


void tienet_slave_daemon(int port) {
  struct sockaddr_in	master, slave;
  socklen_t		addrlen;
  fd_set		readfds;
  short			op;
  unsigned int		size, res_len, data_max, disconnect, ind;
  int			master_socket, slave_socket;
  uint64_t		rays_fired;
  void			*data, *res_buf;
  tie_t			tie;
#if TN_COMPRESSION
  int			comp_max;
  void			*comp_buf;
  unsigned long		dest_len;
#endif


  /* Initialize res_buf to NULL for realloc'ing */
  res_buf = NULL;

  if((slave_socket = socket(AF_INET, SOCK_STREAM, 0)) <= 0) {
    fprintf(stderr, "unable to create socket, exiting.\n");
    exit(1);
  }

  addrlen = sizeof(struct sockaddr_in);
  slave.sin_family = AF_INET;
  slave.sin_addr.s_addr = INADDR_ANY;
  slave.sin_port = htons(port);

  if(bind(slave_socket, (struct sockaddr*)&slave, addrlen)) {
    fprintf(stderr, "socket already bound, exiting.\n");
    exit(1);
  }

  /* listen for connections */
  listen(slave_socket, 3);

  addrlen = sizeof(slave);

  /* Handle connections from master */
  FD_ZERO(&readfds);
  FD_SET(slave_socket, &readfds);
  while(1) {
    disconnect = 0;
    select(slave_socket+1, &readfds, NULL, NULL, NULL);

    /* Accept connection from master */
    master_socket = accept(slave_socket, (struct sockaddr*)&master, &addrlen);

    /* receive endian of master */
    tienet_recv(master_socket, &tienet_endian, sizeof(short), 0);
    tienet_endian = tienet_endian == 1 ? 0 : 1;

    /* send version key, master will respond whether to continue or not */
    tienet_send(master_socket, &tienet_slave_ver_key, sizeof(int), tienet_endian);

    /* If version mismatch then exit, under normal conditions we get TN_OP_OKAY */
    tienet_recv(master_socket, &op, sizeof(short), tienet_endian);
    if(op == TN_OP_COMPLETE)
      return;

    /* Request Work Unit */
    op = TN_OP_REQWORK;
    tienet_send(master_socket, &op, sizeof(short), tienet_endian);

    data_max = 0;
    data = NULL;
#if TN_COMPRESSION
    comp_max = 0;
    comp_buf = NULL;
#endif

    if(master_socket >= 0) {
      do {
        /* Fetch Work Unit */
        tienet_recv(master_socket, &op, sizeof(short), tienet_endian);
        if(op == TN_OP_SHUTDOWN) {
          close(master_socket);
          close(slave_socket);
          tienet_slave_fcb_free();
          exit(0);
        } else if(op == TN_OP_COMPLETE) {
          close(master_socket);
          tienet_slave_fcb_free();
        } else if(op == TN_OP_MESSAGE) {
          unsigned int mesg_len;
          void *mesg;

          /* send this message to the application */
          tienet_recv(master_socket, &mesg_len, sizeof(unsigned int), tienet_endian);
          mesg = malloc(mesg_len);
          tienet_recv(master_socket, mesg, mesg_len, 0);
          tienet_slave_fcb_mesg(mesg, mesg_len);
          free(mesg);
        } else if(op == TN_OP_PREP) {
          /* This can be done better */

          /* Clean and Reinitialize TIE */
          tienet_slave_fcb_free();

          if(tienet_slave_prep(master_socket, &tie)) {
            fprintf(stderr, "version mismatch, exiting.\n");
            close(master_socket);
            exit(0);
          }

          /* Now that we've been updated, Request a Work Unit */
          op = TN_OP_REQWORK;
          tienet_send(master_socket, &op, sizeof(short), tienet_endian);
        } else {
          disconnect += tienet_recv(master_socket, &size, sizeof(unsigned int), tienet_endian);

          if(size > data_max) {
            data_max = size;
            data = realloc(data, size);
          }

          /* Process work and Generate Results */
          disconnect += tienet_recv(master_socket, data, size, 0);

          if(!disconnect)
            tienet_slave_fcb_work(&tie, data, size, &res_buf, &res_len);

          /* Send Result Back, length of: result + op_code + rays_fired + result_length + compression_length */
          if(res_len+sizeof(short)+sizeof(uint64_t)+sizeof(int)+sizeof(unsigned int) > data_max) {
            data_max = res_len+sizeof(short)+sizeof(uint64_t)+sizeof(int)+sizeof(unsigned int);
            data = realloc(data, data_max);
          }

          ind = 0;
          /* Pack Operation Code */
          op = TN_OP_RESULT;
          if(tienet_endian) tienet_flip(&op, &op, sizeof(short));
          memcpy(&((char*)data)[ind], &op, sizeof(short));
          if(tienet_endian) tienet_flip(&op, &op, sizeof(short));
          ind += sizeof(short);

          /* Pack total number of rays fired */
          rays_fired = tie.rays_fired;
          if(tienet_endian) tienet_flip(&rays_fired, &rays_fired, sizeof(uint64_t));
          memcpy(&((char*)data)[ind], &rays_fired, sizeof(uint64_t));
          ind += sizeof(uint64_t);

          /* Pack Result Length */
          if(tienet_endian) tienet_flip(&res_len, &res_len, sizeof(unsigned int));
          memcpy(&((char*)data)[ind], &res_len, sizeof(unsigned int));
          if(tienet_endian) tienet_flip(&res_len, &res_len, sizeof(unsigned int));
          ind += sizeof(unsigned int);

#if TN_COMPRESSION
          /* Compress the result buffer */
          if(comp_max < res_len) {
            comp_buf = realloc(comp_buf, res_len+32);	/* 32 bytes of extra padding for zlib */
            comp_max = res_len;
          }

          dest_len = comp_max+32;
          compress(comp_buf, &dest_len, res_buf, res_len);
          size = (unsigned int)dest_len;

          /* Pack Compressed Result Length */
          if(tienet_endian) tienet_flip(&size, &size, sizeof(unsigned int));
          memcpy(&((char*)data)[ind], &size, sizeof(unsigned int));
          if(tienet_endian) tienet_flip(&size, &size, sizeof(unsigned int));
          ind += sizeof(unsigned int);

          /* Pack Compressed Result Data */
          memcpy(&((char*)data)[ind], comp_buf, size);
          ind += size;
#else
          /* Pack Result Data */
          memcpy(&((char*)data)[ind], res_buf, res_len);
          ind += res_len;
#endif
          disconnect += tienet_send(master_socket, data, ind, 0);
        }
      } while(op != TN_OP_COMPLETE && !disconnect);

      /* Free Data When Master disconnects */
      if(disconnect) {
        close(master_socket);
        tienet_slave_fcb_free();
      }
    }

    /* rebuild select list */
    FD_SET(slave_socket, &readfds);
  }

  free(data);
  free(res_buf);
#if TN_COMPRESSION
  free(comp_buf);
#endif
}


int tienet_slave_prep(int slave_socket, tie_t *tie) {
  /*
  * Process and prep application data.
  * Passing the slave socket allows slave to process the data
  * on demand instead of requiring memory to spike by having it
  * all stored in one big data glob.
  */
  tienet_slave_fcb_init(tie, slave_socket);

  tie_prep(tie);
  return(0);
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
