/*                        M A S T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
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
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "master.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
/*** lib common ***/
#include "canim.h"
#include "cdb.h"
#include "pack.h"
/*** lib util ***/
#include "image.h"
#include "umath.h"
/**** rise ****/
#include "dispatcher.h"
#include "post.h"
#include "rise.h"
/*** Networking Includes ***/
#include "tienet.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#if RISE_USE_COMPRESSION
  #include <zlib.h>
#endif


/* socket structure */
typedef struct rise_master_socket_s {
  int num;
  tienet_sem_t update_sem;
  struct rise_master_socket_s *prev;
  struct rise_master_socket_s *next;
} rise_master_socket_t;


void rise_master(int port, int obs_port, char *proj, char *list, char *exec, int interval);
void rise_master_result(void *res_buf, int res_len);
void* rise_master_networking(void *ptr);

int rise_master_tile_num;
int rise_master_work_ind;

tienet_sem_t rise_master_frame_sem;

void *rise_master_frame;

pthread_t rise_master_networking_thread;
int rise_master_active_connections, rise_master_alive;
rise_master_socket_t *rise_master_socklist;
common_db_t db;


void rise_master(int port, int obs_port, char *proj, char *list, char *exec, int interval) {
  int i, app_size;
  void *app_data;
  char image_name[16];


  rise_master_work_ind = 0;
  tienet_sem_init(&rise_master_frame_sem, 0);

  /* initialize the semaphore for frame updates */
  rise_master_active_connections = 0;

  rise_master_alive = 1;

  /* Parse Env Data */
  common_db_load(&db, proj);

  /* Initialize tienet master */
  rise_master_tile_num = (db.env.img_w * db.env.img_h) / (db.env.tile_w * db.env.tile_h);
  tienet_master_init(port, rise_master_result, list, exec, 10, RISE_VER_KEY);

  rise_master_frame = malloc(4 * sizeof(tfloat) * db.env.img_w * db.env.img_h);
  memset(rise_master_frame, 0, 4 * sizeof(tfloat) * db.env.img_w * db.env.img_h);

  /* Launch a thread to handle networking */
  pthread_create(&rise_master_networking_thread, NULL, rise_master_networking,&obs_port);

  /* Initialize the work dispatcher */
  rise_dispatcher_init();

  /* Render Frame */
  for(i = 0; i < db.anim.frame_num && rise_master_alive; i++) {
    printf("Preparing frame #%d of %d\n", i+1, db.anim.frame_num);

    /* Parse and pack the application data */
    app_size = common_pack(&db, &app_data, proj);
    tienet_master_prep(app_data, app_size);

    /* Fill the work buffer */
    rise_dispatcher_generate(&db, NULL, 0);

    /* Wait for frame to finish */
    tienet_sem_wait(&rise_master_frame_sem);
#if 1
    {
      unsigned char *image24;

      image24 = (unsigned char *)malloc(3 * db.env.img_w * db.env.img_h);
      sprintf(image_name, "image_%.4d.ppm", i);
      rise_post_process(rise_master_frame, db.env.img_w, db.env.img_h);
      util_image_convert_128to24(image24, rise_master_frame, db.env.img_w, db.env.img_h);
      util_image_save_ppm(image_name, image24, db.env.img_w, db.env.img_h);
      free(image24);
    }
#endif
    free(app_data);
  }

  /* Wait for the tienet master work buffer to empty and for all the results to come back */
  tienet_master_wait();

  /* Shutdown */
  tienet_master_shutdown();

  /* Free network data */
  tienet_master_free();

  /* Free the dispatcher data */
  rise_dispatcher_free();

  free(rise_master_frame);

#if 0
  /* End the networking thread */
  pthread_join(rise_master_networking_thread, NULL);
#endif
}


void rise_master_result(void *res_buf, int res_len) {
  rise_master_socket_t *sock;
  common_work_t work;
  unsigned char *rgb_data;
  int i, ind;

  memcpy(&work, res_buf, sizeof(common_work_t));
  rise_master_work_ind++;

  rgb_data = &((unsigned char *)res_buf)[sizeof(common_work_t)];

  ind = 0;
  for(i = work.orig_y; i < work.orig_y + work.size_y; i++) {
    memcpy(&((char *)rise_master_frame)[4 * sizeof(tfloat) * (work.orig_x + i * db.env.img_w)], &rgb_data[ind], 4*sizeof(tfloat)*work.size_x);
    ind += 4 * sizeof(tfloat) * work.size_x;
  }

  /* Progress Indicator */
  printf("Progress: %.1f%%\r", 100 * (tfloat)rise_master_work_ind / (tfloat)rise_master_tile_num);
  fflush(stdout);
/*printf("result: %d %d\n", work.orig_x, work.orig_y); */

  /* post update to all nodes that a tile has come in */
  for(sock = rise_master_socklist; sock; sock = sock->next) {
    if(sock->next) {
      if(!sock->update_sem.val)
        tienet_sem_post(&(sock->update_sem));
    }
  }

  /* Draw the frame to the screen */
  if(rise_master_work_ind == rise_master_tile_num) {
    /* Save image to disk */
    tienet_sem_post(&rise_master_frame_sem);

    rise_master_work_ind = 0;
  }
}


void* rise_master_networking(void *ptr) {
  rise_master_socket_t *sock, *tmp;
  struct sockaddr_in master, observer;
  fd_set readfds;
  int port, master_socket, highest_fd, new_socket, error;
  tfloat cam[8];
  unsigned int addrlen;
  unsigned char op;
  short endian;
  void *frame24;
#if RISE_USE_COMPRESSION
  void *comp_buf;
#endif


  port = *(int *)ptr;
  frame24 = malloc(3 * db.env.img_w * db.env.img_h);


#if RISE_USE_COMPRESSION
  comp_buf = malloc(db.env.img_w * db.env.img_h*3);
#endif

  /* create a socket */
  if((master_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "unable to create socket, exiting.\n");
    exit(1);
  }

  /* initialize socket list */
  rise_master_socklist = (rise_master_socket_t *)malloc(sizeof(rise_master_socket_t));
  rise_master_socklist->next = NULL;
  rise_master_socklist->prev = NULL;
  rise_master_socklist->num = master_socket;

  highest_fd = master_socket;


  /* server address */
  master.sin_family = AF_INET;
  master.sin_addr.s_addr = htonl(INADDR_ANY);
  master.sin_port = htons(port);

  /* bind socket */
  if(bind(master_socket, (struct sockaddr *)&master, sizeof(master)) < 0) {
    fprintf(stderr, "socket already bound, exiting.\n");
    exit(1);
  }

  FD_ZERO(&readfds);
  FD_SET(master_socket, &readfds);

  /* listen for connections */
  listen(rise_master_socklist->num, 3);

  addrlen = sizeof(observer);
  rise_master_active_connections = 0;

  while(rise_master_alive) {
    /* wait for some network activity */
    select(highest_fd+1, &readfds, NULL, NULL, NULL);
    /* cycle through each socket and address the activity */
    for(sock = rise_master_socklist; sock; sock = sock->next) {
      /* check if activity was on this socket */
      if(FD_ISSET(sock->num, &readfds)) {
        if(sock->num == master_socket) {
          /* new connection */
          new_socket = accept(master_socket, (struct sockaddr *)&observer, &addrlen);
          if(new_socket >= 0) {
            tmp = rise_master_socklist;
            rise_master_socklist = (rise_master_socket_t *)malloc(sizeof(rise_master_socket_t));
            rise_master_socklist->num = new_socket;
            rise_master_socklist->next = tmp;
            rise_master_socklist->prev = NULL;
            tienet_sem_init(&(rise_master_socklist->update_sem), 0);
            tmp->prev = rise_master_socklist;
            if(new_socket > highest_fd)
              highest_fd = new_socket;
            rise_master_active_connections++;
          }
        } else {
          op = 255;
          /* observer communication */
          error = tienet_recv(sock->num, &op, 1, 0);
          /* remove socket from pool if there's an error, i.e slave disconnected */
          if(error || op == RISE_NET_OP_QUIT) {
            tmp = sock;
            if(sock->prev)
              sock->prev->next = sock->next;
            /* master is always last, no need to check for sock->next next */
            sock->next->prev = sock->prev;
            if(sock == rise_master_socklist)
              rise_master_socklist = rise_master_socklist->next;
            close(sock->num);
            sock = sock->next;
            free(tmp);
            rise_master_active_connections--;
          } else {
            switch(op) {
              case RISE_NET_OP_INIT:
                /* Send screen width and height */
                endian = 1;
                tienet_send(sock->num, &endian, sizeof(short), 0);
                tienet_send(sock->num, &db.env.img_w, sizeof(int), 0);
                tienet_send(sock->num, &db.env.img_h, sizeof(int), 0);
                tienet_send(sock->num, &common_pack_trinum, sizeof(int), 0);
                break;

              case RISE_NET_OP_FRAME:
                tienet_sem_wait(&(sock->update_sem));
                util_image_convert_128to24(frame24, rise_master_frame, db.env.img_w, db.env.img_h);

#if RISE_USE_COMPRESSION
                {
                  unsigned long dest_len;
                  int comp_size;
                  dest_len = 3 * db.env.img_w * db.env.img_h;

                  /* frame data */
                  compress(comp_buf, &dest_len, frame24, 3 * db.env.img_w * db.env.img_h);
                  comp_size = dest_len;
                  tienet_send(sock->num, &comp_size, sizeof(int), 0);
                  tienet_send(sock->num, comp_buf, comp_size, 0);
                }
#else
                /* frame data */
                tienet_send(sock->num, frame24, 3 * db.env.img_w * db.env.img_h, 0);
#endif
                break;

              case RISE_NET_OP_QUIT:
                rise_master_active_connections = 0;
                break;

              case RISE_NET_OP_SHUTDOWN:
                rise_master_alive = 0;
                break;

              default:
                break;
            }
          }
        }
      }
    }

    /* Rebuild select list for next select call */
    highest_fd = 0;
    for(sock = rise_master_socklist; sock; sock = sock->next) {
      if(sock->num > highest_fd)
        highest_fd = sock->num;
      FD_SET(sock->num, &readfds);
    }
  }

#if RISE_USE_COMPRESSION
  free(comp_buf);
#endif


  /* free rise_master_socklist */
  for(sock = rise_master_socklist->next; sock; sock = sock->next)
    free(sock->prev);

  free(frame24);
  return 0;
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
