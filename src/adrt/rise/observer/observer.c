/*                      O B S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file observer.c
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "brlcad_config.h"
#include "observer.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "SDL.h"
#include "rise.h"
#include "display.h"
#include "umath.h"
#include "tienet.h"
/* Networking Includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#if RISE_USE_COMPRESSION
  #include <zlib.h>
#endif


typedef struct rise_observer_net_info_s {
  struct hostent master;
  int port;
} rise_observer_net_info_t;


void rise_observer(char *host, int port);
void* rise_observer_networking(void *ptr);
void rise_observer_event_loop(void);

pthread_t rise_observer_networking_thread;

pthread_mutex_t event_mut;
int screen_w, screen_h, rise_observer_trinum;
short rise_observer_endian, rise_observer_alive, rise_observer_master_shutdown;
tienet_sem_t rise_observer_sdlinit_sem;
tienet_sem_t rise_observer_sdlready_sem;


void rise_observer(char *host, int port) {
  rise_observer_net_info_t ni;

  /* server address */
  if(gethostbyname(host)) {
    ni.master = gethostbyname(host)[0];
  } else {
    fprintf(stderr, "hostname %s unknown, exiting.\n", host);
    exit(1);
  }
  ni.port = port;

  rise_observer_trinum = 0;

  pthread_mutex_init(&event_mut, 0);

  rise_observer_alive = 1;
  rise_observer_master_shutdown = 0;

  tienet_sem_init(&rise_observer_sdlinit_sem, 0);
  tienet_sem_init(&rise_observer_sdlready_sem, 0);

  /* Launch a thread to handle events */
  pthread_create(&rise_observer_networking_thread, NULL, rise_observer_networking, &ni);

  /* Process events */
  tienet_sem_wait(&rise_observer_sdlinit_sem);
  rise_observer_event_loop();

  /* JOIN EVENT HANDLING THREAD */
  pthread_join(rise_observer_networking_thread, NULL);
}


void* rise_observer_networking(void *ptr) {
  rise_observer_net_info_t *ni;
  struct timeval start, cur;
  int sockd, frame_num;
  struct sockaddr_in my_addr, srv_addr;
  unsigned int addrlen;
  unsigned char op;
  char string[255];
  void *frame;
#if RISE_USE_COMPRESSION
  void *comp_buf;
#endif

  ni = (rise_observer_net_info_t *)ptr;

  /* create a socket */
  if((sockd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Socket creation error");
    exit(1);
  }

  /* client address */
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = INADDR_ANY;
  my_addr.sin_port = htons(0);

  srv_addr.sin_family = ni->master.h_addrtype;
  memcpy((char*)&srv_addr.sin_addr.s_addr, ni->master.h_addr_list[0], ni->master.h_length);
  srv_addr.sin_port = htons(ni->port);

  if(bind(sockd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
    fprintf(stderr, "unable to bind socket, exiting.\n");
    exit(1);
  }

  /* connect to master */
  if(connect(sockd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
    fprintf(stderr, "cannot connect to master, exiting.\n");
    exit(1);
  }

  addrlen = sizeof(srv_addr);

  /* send version and get endian info */
  op = RISE_NET_OP_INIT;
  tienet_send(sockd, &op, 1, rise_observer_endian);
  tienet_recv(sockd, &rise_observer_endian, sizeof(short), 0);
  rise_observer_endian = rise_observer_endian == 1 ? 0 : 1;
  tienet_recv(sockd, &screen_w, sizeof(int), rise_observer_endian);
  tienet_recv(sockd, &screen_h, sizeof(int), rise_observer_endian);
  tienet_recv(sockd, &rise_observer_trinum, sizeof(int), rise_observer_endian);

  /* Screen size is known.  Initialize SDL and continue once it's ready */
  tienet_sem_post(&rise_observer_sdlinit_sem);
  tienet_sem_wait(&rise_observer_sdlready_sem);

  /* Allocate memory for frame buffer */
  frame = malloc(screen_w*screen_h*3);
#if RISE_USE_COMPRESSION
  comp_buf = malloc(screen_w*screen_h*3);
#endif

  frame_num = 0;
  gettimeofday(&start, NULL);

  while(rise_observer_alive) {
    if(rise_observer_master_shutdown) {
      op = RISE_NET_OP_SHUTDOWN;
      tienet_send(sockd, &op, 1, rise_observer_endian);
      rise_observer_alive = 0;
    } else {
      op = RISE_NET_OP_FRAME;
      tienet_send(sockd, &op, 1, rise_observer_endian);

      /* get frame data */
#if RISE_USE_COMPRESSION
      {
        unsigned long dest_len;
        int comp_size;

        tienet_recv(sockd, &comp_size, sizeof(int), rise_observer_endian);
        tienet_recv(sockd, comp_buf, comp_size, 0);
        dest_len = screen_w*screen_h*3;
        uncompress(frame, &dest_len, comp_buf, (unsigned long)comp_size);
      }
#else
      tienet_recv(sockd, frame, 3 * screen_w * screen_h, 0);
#endif
    }

    util_display_draw(frame);

    /* Text Overlay */
    util_display_draw(frame);
    sprintf(string, "Triangles: %.0fk", (tfloat)rise_observer_trinum / (tfloat)1024);
    util_display_text(string, 0, 0, UTIL_JUSTIFY_RIGHT, UTIL_JUSTIFY_BOTTOM);

    sprintf(string, "RES: %dx%d", screen_w, screen_h);
    util_display_text(string, 0, 0, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_BOTTOM);

    util_display_flip();

    frame_num++;
  }

  /*
  * Send a message to the master that this observer is quitting.
  */
  op = RISE_NET_OP_QUIT;
  tienet_send(sockd, &op, 1, rise_observer_endian);

  free(frame);
#if RISE_USE_COMPRESSION
  free(comp_buf);
#endif
  close(sockd);

  return(NULL);
}


void rise_observer_event_loop() {
  SDL_Event event;
  TIE_3 vec, vec2, vec3;


  util_display_init(screen_w, screen_h);
  tienet_sem_post(&rise_observer_sdlready_sem);

  /* Loop waiting for ESC+Mouse_Button */
  while(SDL_WaitEvent(&event) >= 0) {
    pthread_mutex_lock(&event_mut);
    switch(event.type) {
      case SDL_KEYDOWN:
        switch(event.key.keysym.sym) {
          case SDLK_q: /* quit */
            printf("Detaching from master and exiting.\n");
            rise_observer_alive = 0;
            return;
            break;


          case SDLK_F12: /* Server Shutdown and quit*/
            printf("Shutting down master and exiting.\n");
            rise_observer_master_shutdown = 1;
            return;
            break;


          default:
            break;
        }

      case SDL_MOUSEBUTTONDOWN:
        break;

      case SDL_MOUSEMOTION:
        if(event.motion.state) {
        }
        break;

      default:
        break;
    }

    pthread_mutex_unlock(&event_mut);
  }
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
