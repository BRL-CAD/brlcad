/*                        M A S T E R . C
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
/** @file master.c
 *
 * Author -
 *   Justin Shumaker
 *
 */

#include "master.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "SDL.h"		/* SDL */
#include "display.h"		/* Display utilities */
#include "canim.h"		/* Animation Stuff */
#include "dispatcher.h"		/* Dispatcher that creates work units */
#include "cdb.h"		/* Common structs and stuff */
#include "compnet.h"		/* Component Networking, Sends Component Names via Network */
#include "isst.h"		/* ISST Defines */
#include "image.h"		/* Image import/export utilities */
#include "pack.h"		/* Data packing for transport to nodes */
#include "isst_struct.h"	/* isst common structs */
#include "tienet.h"		/* Networking stuff */
#include "umath.h"		/* Extended math utilities */
#include "isst_python.h"	/* Python code Interpreter */
/* Networking Includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#if ISST_USE_COMPRESSION
  #include <zlib.h>
#endif


/* socket structure */
typedef struct isst_master_socket_s {
  int num;
  int controller;
  int active;
  tienet_sem_t frame_sem;
  struct isst_master_socket_s *prev;
  struct isst_master_socket_s *next;
} isst_master_socket_t;


void isst_master(int port, int obs_port, char *proj, char *list, char *exec, char *comp_host);
void* isst_master_networking(void *ptr);
void isst_master_result(void *res_buf, int res_len);
void isst_master_update(void);
void isst_master_process_events(isst_event_t *event_queue, uint8_t event_num, isst_master_socket_t *sock);


/***** GLOBALS *****/
int isst_master_tile_num;
TIE_3 isst_master_camera_position;
TIE_3 isst_master_camera_focus;
TIE_3 isst_master_cor; /* center of rotation */
TIE_3 isst_master_shot_position;
TIE_3 isst_master_shot_direction;
TIE_3 isst_master_in_hit;
TIE_3 isst_master_out_hit;
tfloat isst_master_spall_angle;
tfloat isst_master_camera_azimuth;
tfloat isst_master_camera_elevation;

common_db_t db;
isst_master_socket_t *isst_master_socklist;
tienet_sem_t isst_master_frame_wait_sem;
void *isst_master_observer_frame;
pthread_mutex_t isst_master_observer_frame_mut;
void *rgb_frame;
int frame_ind;
char isst_master_slave_data[64];
int isst_master_slave_data_len;
pthread_t isst_master_networking_thread;
tfloat isst_master_scale;
int isst_master_active_connections;
int isst_master_alive;
unsigned char isst_master_rm;
pthread_mutex_t isst_master_update_mut;
int isst_master_mouse_grab;
int isst_master_shift_enabled;
/*******************/


static void isst_master_setup() {
  tfloat celev;

  isst_master_active_connections = 0;

  isst_master_alive = 1;
  isst_master_scale = 0.01;
  isst_master_mouse_grab = 0;
  isst_master_shift_enabled = 0;
  isst_master_spall_angle = 10;
  MATH_VEC_SET(isst_master_in_hit, 0, 0, 0);
  MATH_VEC_SET(isst_master_out_hit, 0, 0, 0);

  MATH_VEC_SET(isst_master_camera_position, 10, 10, 10);
  MATH_VEC_SET(isst_master_shot_position, 0, 0, 0);
  isst_master_camera_focus = isst_master_camera_position;

  celev = cos(35 * MATH_DEG2RAD);
  isst_master_camera_focus.v[0] -= cos(45 * MATH_DEG2RAD) * celev;
  isst_master_camera_focus.v[1] -= sin(45 * MATH_DEG2RAD) * celev;
  isst_master_camera_focus.v[2] -= sin(35 * MATH_DEG2RAD);

  isst_master_camera_azimuth = 45;
  isst_master_camera_elevation = 35;

  isst_master_rm = RENDER_METHOD_PHONG;

  frame_ind = 0;

  rgb_frame = malloc(3 * db.env.img_w * db.env.img_h);
  memset(rgb_frame, 0, 3 * db.env.img_w * db.env.img_h);

  isst_master_observer_frame = malloc(3 * db.env.img_w * db.env.img_h);
}


void isst_master(int port, int obs_port, char *proj, char *list, char *exec, char *comp_host) {
  int frame_num, app_size;
  void *app_data;
  struct timeval start, cur;


  /* Parse Env Data */
  common_db_load(&db, proj);

  /* Setup defaults */
  isst_master_setup();

  /* Initialize Python Processor */
  isst_python_init();

  /* Mutex for everytime the master builds update data to send to nodes */
  pthread_mutex_init(&isst_master_update_mut, 0);

  /*
  * This mutex exists to prevent the observer from reading a frame while
  * the result function is writing to it, which would otherwise result in
  * the appearance of single buffering behavior for moderately high frame rates.
  */
  pthread_mutex_init(&isst_master_observer_frame_mut, 0);

  /* Initialize tienet master */
  isst_master_tile_num = (db.env.img_w * db.env.img_h) / (db.env.tile_w * db.env.tile_h);
  tienet_master_init(port, isst_master_result, list, exec, 5, ISST_VER_KEY);

  /* Launch a thread to handle networking */
  pthread_create(&isst_master_networking_thread, NULL, isst_master_networking,&obs_port);

  /* Connect to the component Server */
  isst_compnet_connect(comp_host, ISST_COMPNET_PORT);

  /* Parse and pack the application data */
  printf("loading scene... ");
  fflush(stdout);
  app_size = common_pack(&db, &app_data, proj);
  printf("done.\n");

  tienet_master_prep(app_data, app_size);

  /* Initialize the work dispatcher */
  isst_dispatcher_init();

  /* Data for computing fps */
  frame_num = 0;
  gettimeofday(&start, NULL);

  /*
  * Double buffer semaphore, never let more than 1 frame into the future
  * to be computed at any given time.  Without this, for 3 given frames
  * there is a possibility that while frame_0 and frame_1 are being computed
  * by the compute nodes, that frame_2 might be worked on by a compute node.
  * because frame indices are 0 or 1, hence double buffered, frame_0 and frame_2
  * have the same frame id and can therefore have resulting tiles over-write
  * one another resulting in a jittery image.
  */
  tienet_sem_init(&isst_master_frame_wait_sem, 1);

  while(isst_master_alive) {
    /* Double buffer semaphore, do not get too far ahead */
    tienet_sem_wait(&isst_master_frame_wait_sem);

    /* Update Camera Position */
    isst_master_update();

    /* Fill the work buffer */
    isst_dispatcher_generate(&db, isst_master_slave_data, isst_master_slave_data_len);

#if 0
    frame_num++;
    if(!(frame_num % 7)) {
      gettimeofday(&cur, NULL);
      printf("FPS: %.3f  SLAVES: %d    \r",
            (tfloat)(frame_num) / ((cur.tv_sec + (tfloat)cur.tv_usec/1000000.0) - (start.tv_sec + (tfloat)start.tv_usec/1000000.0)),
            tienet_master_socket_num);
      start = cur;
      frame_num = 0;
      fflush(stdout);
    }
#endif
  }

  /* Wait for the tienet master work buffer to empty and for all the results to come back */
  tienet_master_wait();

  /* Shutdown */
  tienet_master_shutdown();

  /* Free network data */
  tienet_master_free();

  /* Free the dispatcher data */
  isst_dispatcher_free();

  free(rgb_frame);

  /* End the networking thread */
  pthread_join(isst_master_networking_thread, NULL);

  isst_python_free();
}


void isst_master_result(void *res_buf, int res_len) {
  isst_master_socket_t *sock;
  common_work_t work;
  unsigned char *rgb_data;
  int i, ind, num;
  short frame;
  char name[256];
  unsigned char c;


  /* Work unit data */
  memcpy(&work, res_buf, sizeof(common_work_t));

  if(work.size_x == 0) {  /* ugly hack until I get a better identifier in place */
    char *mesg;
    short slop;

    /* Read and advance to data */
    ind = sizeof(common_work_t);

    /* first hit */
    memcpy(&isst_master_in_hit, &((unsigned char *)res_buf)[ind], sizeof(TIE_3));
    ind += sizeof(TIE_3);

    /* last hit */
    memcpy(&isst_master_out_hit, &((unsigned char *)res_buf)[ind], sizeof(TIE_3));
    ind += sizeof(TIE_3);

printf("in_hit: [%f, %f, %f], out_hit: [%f, %f, %f]\n", isst_master_in_hit.v[0], isst_master_in_hit.v[1], isst_master_in_hit.v[2], isst_master_out_hit.v[0], isst_master_out_hit.v[1], isst_master_out_hit.v[2]);

    /* Set the Center of of Rotation */
    MATH_VEC_ADD(isst_master_cor, isst_master_in_hit, isst_master_out_hit);
    MATH_VEC_MUL_SCALAR(isst_master_cor, isst_master_cor, 0.5);

    /* number of meshes */
    memcpy(&num, &((unsigned char *)res_buf)[ind], sizeof(int));
    ind += sizeof(int);

    /* reset the component server to collapse all fields */
    isst_compnet_reset();

    for(i = 0; i < num; i++) {
      memcpy(&c, &((unsigned char *)res_buf)[ind], 1);
      ind += 1;
      memcpy(name, &((unsigned char *)res_buf)[ind], c);
      ind += c;
printf("component[%d]: %s\n", i, name);
      isst_compnet_update(name, 1);
    }

    /* Send a message to update hitlist */
    mesg = malloc(sizeof(short) + res_len - sizeof(common_work_t) - 6*sizeof(tfloat));

    ind = 0;
    slop = ISST_OP_SHOT;
    memcpy(&((char *)mesg)[ind], &slop, sizeof(short));
    ind += sizeof(short);

    memcpy(&((char *)mesg)[ind], &((unsigned char *)res_buf)[sizeof(common_work_t) + 6*sizeof(tfloat)], res_len - sizeof(common_work_t) - 6*sizeof(tfloat));
    ind += res_len - sizeof(common_work_t) - 6*sizeof(tfloat);

    tienet_master_broadcast(mesg, ind);
    free(mesg);
  } else {
    /* Pointer to RGB Data */
    rgb_data = &((unsigned char *)res_buf)[sizeof(common_work_t)];

    frame_ind++;

    /* Copy the tile into the image */
    ind = 0;
    for(i = work.orig_y; i < work.orig_y + work.size_y; i++) {
      memcpy(&((char *)rgb_frame)[3 * (work.orig_x + i * db.env.img_w)], &rgb_data[ind], 3*work.size_y);
      ind += 3*work.size_y;
    }


    /* Image is complete, draw the frame. */
    if(frame_ind == isst_master_tile_num) {
      frame_ind = 0;

      /* Copy this frame to the observer frame buffer */
      pthread_mutex_lock(&isst_master_observer_frame_mut);
      memcpy(isst_master_observer_frame, rgb_frame, 3 * db.env.img_w * db.env.img_h);
      pthread_mutex_unlock(&isst_master_observer_frame_mut);

      /* Allow the next frame to be computed */
      tienet_sem_post(&isst_master_frame_wait_sem);

      /* Alert the observers that a new frame is available for viewing */
      for(sock = isst_master_socklist; sock; sock = sock->next) {
        if(sock->next)
          if(!sock->frame_sem.val)
            tienet_sem_post(&(sock->frame_sem));
      }
    }
  }
}


void* isst_master_networking(void *ptr) {
  isst_master_socket_t *sock, *tmp;
  struct sockaddr_in master, observer;
  fd_set readfds;
  int port, master_socket, highest_fd, new_socket, error;
  unsigned int addrlen;
  unsigned char op;
  short endian;
#if ISST_USE_COMPRESSION
  void *comp_buf;
#endif


  port = *(int *)ptr;


#if ISST_USE_COMPRESSION
  comp_buf = malloc(3 * db.env.img_w * db.env.img_h + 1024);
#endif

  /* create a socket */
  if((master_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "unable to create socket, exiting.\n");
    exit(1);
  }

  /* initialize socket list */
  isst_master_socklist = (isst_master_socket_t *)malloc(sizeof(isst_master_socket_t));
  isst_master_socklist->next = NULL;
  isst_master_socklist->prev = NULL;
  isst_master_socklist->num = master_socket;

  highest_fd = master_socket;


  /* server address */
  master.sin_family = AF_INET;
  master.sin_addr.s_addr = htonl(INADDR_ANY);
  master.sin_port = htons(port);

  /* bind socket */
  if(bind(master_socket, (struct sockaddr *)&master, sizeof(master)) < 0) {
    fprintf(stderr, "observer socket already bound, exiting.\n");
    exit(1);
  }

  FD_ZERO(&readfds);
  FD_SET(master_socket, &readfds);

  /* listen for connections */
  listen(isst_master_socklist->num, 3);

  addrlen = sizeof(observer);
  isst_master_active_connections = 0;

  while(isst_master_alive || isst_master_active_connections) {
    /* wait for some network activity */
    select(highest_fd+1, &readfds, NULL, NULL, NULL);
    /* cycle through each socket and address the activity */
    for(sock = isst_master_socklist; sock; sock = sock->next) {
      /* if no longer alive then mark each of the active connections as no longer active */
      if(!isst_master_alive)
        sock->active = 0;

      /* check if activity was on this socket */
      if(FD_ISSET(sock->num, &readfds)) {
        if(sock->num == master_socket) {
          /* new connection */
          new_socket = accept(master_socket, (struct sockaddr *)&observer, &addrlen);
          if(new_socket >= 0) {
            tmp = isst_master_socklist;
            isst_master_socklist = (isst_master_socket_t *)malloc(sizeof(isst_master_socket_t));
            isst_master_socklist->num = new_socket;
            isst_master_socklist->controller = isst_master_active_connections ? 0 : 1;
            isst_master_socklist->active = 1;
            isst_master_socklist->next = tmp;
            isst_master_socklist->prev = NULL;
            tienet_sem_init(&(isst_master_socklist->frame_sem), 0);
            tmp->prev = isst_master_socklist;
            if(new_socket > highest_fd)
              highest_fd = new_socket;
            isst_master_active_connections++;
          }
        } else {
          op = 255;
          /* observer communication */
          error = tienet_recv(sock->num, &op, 1, 0);

          /* remove socket from pool if there's an error, i.e slave disconnected */
          if(error || op == ISST_NET_OP_QUIT || !sock->active) {
            op = ISST_NET_OP_QUIT;
            tienet_send(sock->num, &op, 1, 0);

            tmp = sock;
            if(sock->prev)
              sock->prev->next = sock->next;
            /* master is always last, no need to check for sock->next next */
            sock->next->prev = sock->prev;
            if(sock == isst_master_socklist)
              isst_master_socklist = isst_master_socklist->next;
            close(sock->num);
            sock = sock->next;
            free(tmp);
            isst_master_active_connections--;

          } else {
            switch(op) {
              case ISST_NET_OP_INIT:
                /* Send screen width and height */
                endian = 1;
                tienet_send(sock->num, &endian, sizeof(short), 0);
                tienet_send(sock->num, &db.env.img_w, sizeof(int), 0);
                tienet_send(sock->num, &db.env.img_h, sizeof(int), 0);
                break;

              case ISST_NET_OP_FRAME:
                tienet_sem_wait(&(sock->frame_sem));

                /* Let observer know everything is okay, continue as usual. */
                op = ISST_NET_OP_NOP;
                tienet_send(sock->num, &op, 1, 0);

                {
                  isst_event_t event_queue[64];
                  uint8_t event_num;

                  /* Get the event Queue and process it */
                  tienet_recv(sock->num, &event_num, sizeof(uint8_t), 0);
                  if(event_num)
                    tienet_recv(sock->num, event_queue, event_num * sizeof(isst_event_t), 0);
                  isst_master_process_events(event_queue, event_num, sock);
                }

#if ISST_USE_COMPRESSION
                {
                  unsigned long dest_len;
                  unsigned int comp_size;
                  dest_len = 3 * db.env.img_w * db.env.img_h + 1024;

                  /* frame data */
                  pthread_mutex_lock(&isst_master_observer_frame_mut);
                  compress(&((char *)comp_buf)[sizeof(unsigned int)], &dest_len, isst_master_observer_frame, 3 * db.env.img_w * db.env.img_h);
                  pthread_mutex_unlock(&isst_master_observer_frame_mut);
                  comp_size = dest_len;
                  memcpy(comp_buf, &comp_size, sizeof(unsigned int));
                  /* int for frame size in bytes followed by actual rgb frame data */
                  tienet_send(sock->num, comp_buf, comp_size + sizeof(unsigned int), 0);
                }
#else
                /* frame data */
                pthread_mutex_lock(&isst_master_observer_frame_mut);
                tienet_send(sock->num, isst_master_observer_frame, db.env.img_w * db.env.img_h * 3, 0);
                pthread_mutex_unlock(&isst_master_observer_frame_mut);
#endif

                /* Send overlay data */
                {
                  isst_overlay_data_t overlay;

                  overlay.camera_position = isst_master_camera_position;
                  overlay.camera_azimuth = isst_master_camera_azimuth;
                  overlay.camera_elevation = isst_master_camera_elevation;
                  overlay.compute_nodes = tienet_master_active_slaves;
                  overlay.in_hit = isst_master_in_hit;
                  overlay.out_hit = isst_master_out_hit;
                  overlay.scale = isst_master_scale;
                  sprintf(overlay.resolution, "%dx%d", db.env.img_w, db.env.img_h);
                  overlay.controller = sock->controller;

                  tienet_send(sock->num, &overlay, sizeof(isst_overlay_data_t), 0);
                }

                /* Lock things down so that isst_master_update data doesn't get tainted */
                pthread_mutex_lock(&isst_master_update_mut);

                pthread_mutex_unlock(&isst_master_update_mut);
                break;

              case ISST_NET_OP_MESG:
                {
                  char *string;
                  int len;

                  string = (char *)malloc(1024);
                  tienet_recv(sock->num, &len, 1, 0);
                  tienet_recv(sock->num, string, len, 0);

                  isst_python_code(string);
                  len = strlen(string) + 1;

                  tienet_send(sock->num, &len, 1, 0);
                  tienet_send(sock->num, string, len, 0);

                  free(string);
                }
                break;


              case ISST_NET_OP_QUIT:
                isst_master_active_connections = 0;
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
    for(sock = isst_master_socklist; sock; sock = sock->next) {
      if(sock->num > highest_fd)
        highest_fd = sock->num;
      FD_SET(sock->num, &readfds);
    }
  }

#if ISST_USE_COMPRESSION
  free(comp_buf);
#endif


  /* free isst_master_socklist */
  for(sock = isst_master_socklist->next; sock; sock = sock->next)
    free(sock->prev);

  return 0;
}


/* additional baggage that goes with each work unit */
void isst_master_update() {
  char op;

  isst_master_slave_data_len = 0;

  /* Lock things down so that isst_master_update data doesn't get tainted */
  pthread_mutex_lock(&isst_master_update_mut);

  /* function */
  op = ISST_OP_RENDER;
  memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], &op, 1);
  isst_master_slave_data_len += 1;

  /* Camera Position */
  memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], isst_master_camera_position.v, sizeof(TIE_3));
  isst_master_slave_data_len += sizeof(TIE_3);

  /* Camera Focus */
  memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], isst_master_camera_focus.v, sizeof(TIE_3));
  isst_master_slave_data_len += sizeof(TIE_3);

  /* Rendering Method and Data */
  memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], &isst_master_rm, 1);
  isst_master_slave_data_len += 1;

  switch(isst_master_rm) {
    case RENDER_METHOD_PLANE:
      memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], &isst_master_shot_position, sizeof(TIE_3));
      isst_master_slave_data_len += sizeof(TIE_3);

      memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], &isst_master_shot_direction, sizeof(TIE_3));
      isst_master_slave_data_len += sizeof(TIE_3);
      break;

    case RENDER_METHOD_SPALL:
      memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], &isst_master_in_hit, sizeof(TIE_3));
      isst_master_slave_data_len += sizeof(TIE_3);

      memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], &isst_master_shot_direction, sizeof(TIE_3));
      isst_master_slave_data_len += sizeof(TIE_3);

      memcpy(&((char *)isst_master_slave_data)[isst_master_slave_data_len], &isst_master_spall_angle, sizeof(tfloat));
      isst_master_slave_data_len += sizeof(tfloat);
      break;

    default:
      break;
  }

  pthread_mutex_unlock(&isst_master_update_mut);
}


void isst_master_process_events(isst_event_t *event_queue, uint8_t event_num, isst_master_socket_t *sock) {
  int i, update;
  TIE_3 vec, vec2, vec3;
  tfloat celev;


  for(i = 0; i < event_num; i++) {
    update = 0;
    switch(event_queue[i].type) {
      printf("event_type: %d\n", event_queue[i].type);
      case SDL_KEYDOWN:
        update = 1;
        switch(event_queue[i].keysym) {
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
            isst_master_shift_enabled = 1;
            break;

          case SDLK_1: /* RENDER_METHOD_PHONG */
            isst_master_rm = RENDER_METHOD_PHONG;
            break;

          case SDLK_2: /* RENDER_METHOD_PLANE */
            isst_master_rm = RENDER_METHOD_PLANE;
            break;

          case SDLK_3: /* RENDER_METHOD_SPALL */
            isst_master_rm = RENDER_METHOD_SPALL;
            break;

          case SDLK_4: /* RENDER_METHOD_COMPONENT */
            isst_master_rm = RENDER_METHOD_COMPONENT;
            break;

          case SDLK_8: /* RENDER_METHOD_DEPTH */
            isst_master_rm = RENDER_METHOD_DEPTH;
            break;

          case SDLK_9: /* RENDER_METHOD_GRID */
            isst_master_rm = RENDER_METHOD_GRID;
            break;

          case SDLK_0: /* RENDER_METHOD_NORMAL */
            isst_master_rm = RENDER_METHOD_NORMAL;
            break;


          case SDLK_UP:
            MATH_VEC_SUB(vec, isst_master_camera_focus, isst_master_camera_position);
            MATH_VEC_MUL_SCALAR(vec, vec, isst_master_scale*10.0);
            MATH_VEC_ADD(isst_master_camera_position, isst_master_camera_position, vec);
            break;

          case SDLK_DOWN:
            MATH_VEC_SUB(vec, isst_master_camera_focus, isst_master_camera_position);
            MATH_VEC_MUL_SCALAR(vec, vec, isst_master_scale*10.0);
            MATH_VEC_SUB(isst_master_camera_position, isst_master_camera_position, vec);
            break;

          case SDLK_LEFT:
            /* strafe left */
            MATH_VEC_SUB(vec, isst_master_camera_focus, isst_master_camera_position);
            MATH_VEC_SET(vec2, 0, 0, 1);
            MATH_VEC_CROSS(vec3, vec2, vec);
            MATH_VEC_MUL_SCALAR(vec3, vec3, (isst_master_scale*10.0));
            MATH_VEC_ADD(isst_master_camera_position, isst_master_camera_position, vec3);
            break;

          case SDLK_RIGHT:
            /* strafe right */
            MATH_VEC_SUB(vec, isst_master_camera_focus, isst_master_camera_position);
            MATH_VEC_SET(vec2, 0, 0, 1);
            MATH_VEC_CROSS(vec3, vec2, vec);
            MATH_VEC_MUL_SCALAR(vec3, vec3, (isst_master_scale*10.0));
            MATH_VEC_SUB(isst_master_camera_position, isst_master_camera_position, vec3);
            break;

          case SDLK_F12: /* Server Shutdown and quit*/
            isst_master_alive = 0;
            break;

          case SDLK_d: /* detach */
            sock->active = 0;
            break;

          case SDLK_e: /* export frame */
#if 0
            pthread_mutex_lock(&isst_observer_gui_mut);
            {
              char filename[32];
              void *image24;
              strcpy(filename, "frame.ppm");
              image24 = malloc(3 * screen_w * screen_h);
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
              util_image_convert_32to24(image24, util_display_screen->pixels, screen_w, screen_h, 0);
#else
              util_image_convert_32to24(image24, util_display_screen->pixels, screen_w, screen_h, 1);
#endif
              util_display_text_input("export_frame", filename, 32);
              util_image_save_ppm(filename, image24, screen_w, screen_h);
            }
            pthread_mutex_unlock(&isst_observer_gui_mut);
#endif
            break;

          case SDLK_f: /* fullscreen toggle */
            /* Reserved for Fullscreen */
            break;

	  case SDLK_g: /* grab mouse */
            isst_master_mouse_grab = isst_master_mouse_grab ^ 1;
	    break;

          case SDLK_KP1: /* front, back */
            {
              tfloat dist;

              /* distance to center of rotation */
              MATH_VEC_SUB(vec, isst_master_camera_position, isst_master_cor);
              MATH_VEC_DOT(dist, vec, vec);
              dist = sqrt(dist);

              isst_master_camera_position = isst_master_cor;
              if(isst_master_shift_enabled) {
                isst_master_camera_position.v[0] -= dist;
                isst_master_camera_azimuth = 180;
                isst_master_camera_elevation = 0;
              } else {
                isst_master_camera_position.v[0] += dist;
                isst_master_camera_azimuth = 0;
                isst_master_camera_elevation = 0;
              }
            }
            break;

          case SDLK_KP3: /* right, left */
            {
              tfloat dist;

              /* distance to center of rotation */
              MATH_VEC_SUB(vec, isst_master_camera_position, isst_master_cor);
              MATH_VEC_DOT(dist, vec, vec);
              dist = sqrt(dist);

              isst_master_camera_position = isst_master_cor;
              if(isst_master_shift_enabled) {
                isst_master_camera_position.v[1] -= dist;
                isst_master_camera_azimuth = 270;
                isst_master_camera_elevation = 0;
              } else {
                isst_master_camera_position.v[1] += dist;
                isst_master_camera_azimuth = 90;
                isst_master_camera_elevation = 0;
              }
            }
            break;

          case SDLK_KP7: /* top, bottom */
            {
              tfloat dist;

              /* distance to center of rotation */
              MATH_VEC_SUB(vec, isst_master_camera_position, isst_master_cor);
              MATH_VEC_DOT(dist, vec, vec);
              dist = sqrt(dist);

              isst_master_camera_position = isst_master_cor;
              if(isst_master_shift_enabled) {
                isst_master_camera_position.v[2] -= dist;
                isst_master_camera_azimuth = 0;
                isst_master_camera_elevation = -90 + 0.01;
              } else {
                isst_master_camera_position.v[2] += dist;
                isst_master_camera_azimuth = 0;
                isst_master_camera_elevation = 90 - 0.01;
              }
            }
            break;

          case SDLK_KP0: /* set camera position and direction to shot position and direction */
            isst_master_camera_position = isst_master_shot_position;
            /* project and unitize shot vector onto xy plane */
            vec = isst_master_shot_direction;
            vec.v[2] = 0;
            MATH_VEC_UNITIZE(vec);

            isst_master_camera_azimuth = fmod(180 + (vec.v[1] < 0 ? 360.0 - acos(vec.v[0])*MATH_RAD2DEG : acos(vec.v[0])*MATH_RAD2DEG), 360.0);
            isst_master_camera_elevation = -asin(isst_master_shot_direction.v[2]) * MATH_RAD2DEG;
            break;


          case SDLK_KP_DIVIDE: /* shotline */
            {
              void *mesg;
              common_work_t work;
              TIE_3 direction;
              int dlen;
              char op;

              /* Queue a work unit for a shot needed for the plane render method */
              mesg = malloc(sizeof(common_work_t) + 1 + 2*sizeof(TIE_3));
              dlen = 0;

              work.orig_x = 0;
              work.orig_y = 0;
              work.size_x = 0;
              work.size_y = 0;

              memcpy(&((char *)mesg)[dlen], &work, sizeof(common_work_t));
              dlen += sizeof(common_work_t);

              /* function */
              op = ISST_OP_SHOT;
              memcpy(&((char *)mesg)[dlen], &op, 1);
              dlen += 1;

              /* position */
              memcpy(&((char *)mesg)[dlen], &isst_master_camera_position, sizeof(TIE_3));
              dlen += sizeof(TIE_3);

              MATH_VEC_SUB(direction, isst_master_camera_focus, isst_master_camera_position);
              MATH_VEC_UNITIZE(direction);

              /* direction */
              memcpy(&((char *)mesg)[dlen], &direction, sizeof(TIE_3));
              dlen += sizeof(TIE_3);

              isst_master_shot_position = isst_master_camera_position;
              isst_master_shot_direction = direction;

              tienet_master_push(mesg, dlen);
              free(mesg);
            }
            break;

          case SDLK_KP_MULTIPLY: /* spawl cone */
            {
              void *mesg;
              common_work_t work;
              TIE_3 direction;
              int dlen;
              char op;

              /* Queue a work unit for a shot needed for the plane render method */
              mesg = malloc(sizeof(common_work_t) + 1 + 2*sizeof(TIE_3) + sizeof(tfloat));
              dlen = 0;

              work.orig_x = 0;
              work.orig_y = 0;
              work.size_x = 0;
              work.size_y = 0;

              memcpy(&((char *)mesg)[dlen], &work, sizeof(common_work_t));
              dlen += sizeof(common_work_t);

              /* function */
              op = ISST_OP_SPALL;
              memcpy(&((char *)mesg)[dlen], &op, 1);
              dlen += 1;

              /* position */
              memcpy(&((char *)mesg)[dlen], &isst_master_camera_position, sizeof(TIE_3));
              dlen += sizeof(TIE_3);

              MATH_VEC_SUB(direction, isst_master_camera_focus, isst_master_camera_position);
              MATH_VEC_UNITIZE(direction);

              /* direction */
              memcpy(&((char *)mesg)[dlen], &direction, sizeof(TIE_3));
              dlen += sizeof(TIE_3);

              /* angle */
              memcpy(&((char *)mesg)[dlen], &isst_master_spall_angle, sizeof(tfloat));
              dlen += sizeof(tfloat);

              isst_master_shot_position = isst_master_camera_position;
              isst_master_shot_direction = direction;

              tienet_master_push(mesg, dlen);
              free(mesg);
            }
            break;


          case SDLK_BACKQUOTE: /* console */
            /* reserved for console in observer */
            break;

          default:
            break;
        }
        break;

      case SDL_KEYUP:
        switch(event_queue[i].keysym) {
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
            isst_master_shift_enabled = 0;
            break;

          default:
            break;
        }

      case SDL_MOUSEBUTTONDOWN:
        update = 1;
        if(event_queue[i].button == SDL_BUTTON_WHEELUP)
          isst_master_scale *= 1.25;

        if(event_queue[i].button == SDL_BUTTON_WHEELDOWN)
          isst_master_scale *= 0.8;
        break;

      case SDL_MOUSEMOTION:
        if(event_queue[i].motion_state && isst_master_mouse_grab) {
	  int dx, dy;

	  dx = -event_queue[i].motion_xrel;
	  dy = -event_queue[i].motion_yrel;

          update = 1;
          if(event_queue[i].button & 1<<(SDL_BUTTON_LEFT-1)) {
            /* backward and forward */
            MATH_VEC_SUB(vec, isst_master_camera_focus, isst_master_camera_position);
            MATH_VEC_MUL_SCALAR(vec, vec, (isst_master_scale*dy));
            MATH_VEC_ADD(isst_master_camera_position, isst_master_camera_position, vec);

            /* strafe */
#if 0
            MATH_VEC_SUB(vec, isst_master_camera_focus, isst_master_camera_position);
            vec2.v[0] = 0;
            vec2.v[1] = 0;
            vec2.v[2] = 1;
            MATH_VEC_CROSS(vec3, vec2, vec);
            MATH_VEC_MUL_SCALAR(vec3, vec3, (isst_master_scale*dx));
            MATH_VEC_ADD(isst_master_camera_position, isst_master_camera_position, vec3);
#endif
          } else if(event_queue[i].button & 1<<(SDL_BUTTON_RIGHT-1)) {
            /* if the shift key is held down then rotate about Center of Rotation */
            if(isst_master_shift_enabled) {
              TIE_3 vec;
              tfloat mag, angle;

              vec.v[0] = isst_master_cor.v[0] - isst_master_camera_position.v[0];
              vec.v[1] = isst_master_cor.v[1] - isst_master_camera_position.v[1];
              vec.v[2] = 0;

#if 0
              MATH_VEC_UNITIZE(vec);
              MATH_VEC_MAGnitude(mag, vec);
#else
              angle = vec.v[0]*vec.v[0] + vec.v[1]*vec.v[1];
              mag = sqrt(angle);
              vec.v[0] /= mag;
              vec.v[1] /= mag;
#endif

              vec.v[0] = vec.v[1] < 0 ? 360.0 - acos(vec.v[0])*MATH_RAD2DEG : acos(vec.v[0])*MATH_RAD2DEG;

              vec.v[0] -= 0.035*dx;
              vec.v[0] *= MATH_DEG2RAD;

              isst_master_camera_position.v[0] = -mag*cos(vec.v[0]) + isst_master_cor.v[0];
              isst_master_camera_position.v[1] = -mag*sin(vec.v[0]) + isst_master_cor.v[1];
              isst_master_camera_azimuth -= 0.035*dx;
            } else {
              isst_master_camera_azimuth += 0.035*dx;
              isst_master_camera_elevation -= 0.035*dy;
            }
          } else if(event_queue[i].button & 1<<(SDL_BUTTON_MIDDLE-1)) {
            isst_master_camera_position.v[2] += isst_master_scale*dy;

            /* strafe */
            MATH_VEC_SUB(vec, isst_master_camera_focus, isst_master_camera_position);
            MATH_VEC_SET(vec2, 0, 0, 1);
            MATH_VEC_CROSS(vec3, vec2, vec);
            MATH_VEC_MUL_SCALAR(vec3, vec3, (isst_master_scale*dx));
            MATH_VEC_ADD(isst_master_camera_position, isst_master_camera_position, vec3);
          }

          /* Keep azimuth position */
          if(isst_master_camera_azimuth < 0)
            isst_master_camera_azimuth += 360;
          if(isst_master_camera_azimuth > 360)
            isst_master_camera_azimuth -= 360;

          /* Keep elevation between -90 and +90 */
          if(isst_master_camera_elevation < -90)
            isst_master_camera_elevation = -90;
          if(isst_master_camera_elevation > 90)
            isst_master_camera_elevation = 90;

        }
        break;

      default:
        break;
    }

    if(update) {
      /* Update the camera data based on the current azimuth and elevation */
      isst_master_camera_focus = isst_master_camera_position;
      celev = cos(isst_master_camera_elevation * MATH_DEG2RAD);
      isst_master_camera_focus.v[0] -= cos(isst_master_camera_azimuth * MATH_DEG2RAD) * celev;
      isst_master_camera_focus.v[1] -= sin(isst_master_camera_azimuth * MATH_DEG2RAD) * celev;
      isst_master_camera_focus.v[2] -= sin(isst_master_camera_elevation * MATH_DEG2RAD);
    }
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
