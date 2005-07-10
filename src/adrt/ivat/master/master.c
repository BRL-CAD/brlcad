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
#include "ivat.h"		/* IVAT Defines */
#include "image.h"		/* Image import/export utilities */
#include "pack.h"		/* Data packing for transport to nodes */
#include "ivat_struct.h"	/* ivat common structs */
#include "tienet.h"		/* Networking stuff */
#include "umath.h"		/* Extended math utilities */
#include "ivat_python.h"	/* Python code Interpreter */
/* Networking Includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#if IVAT_USE_COMPRESSION
  #include <zlib.h>
#endif


/* socket structure */
typedef struct ivat_master_socket_s {
  int num;
  int controller;
  int active;
  tienet_sem_t frame_sem;
  struct ivat_master_socket_s *prev;
  struct ivat_master_socket_s *next;
} ivat_master_socket_t;


void ivat_master(int port, int obs_port, char *proj, char *list, char *exec, char *comp_host);
void* ivat_master_networking(void *ptr);
void ivat_master_result(void *res_buf, int res_len);
void ivat_master_update(void);
void ivat_master_process_events(SDL_Event *event_queue, int event_num, ivat_master_socket_t *sock);


/***** GLOBALS *****/
int ivat_master_tile_num;
TIE_3 ivat_master_camera_pos;
TIE_3 ivat_master_camera_foc;
TIE_3 ivat_master_cor; /* center of rotation */
TIE_3 ivat_master_shot_pos;
TIE_3 ivat_master_shot_dir;
tfloat ivat_master_spall_angle;
TIE_3 ivat_master_in_hit;
TIE_3 ivat_master_out_hit;

void *rgb_frame[2];
int frame_ind[2];
short frame_cur_ind;
short frame_ind_done;
char ivat_master_slave_data[64];
int ivat_master_slave_data_len;
pthread_t ivat_master_networking_thread;
tfloat ivat_master_azim;
tfloat ivat_master_elev;
tfloat ivat_master_scale;
int ivat_master_active_connections;
int ivat_master_alive;
ivat_master_socket_t *ivat_master_socklist;
common_db_t db;
unsigned char ivat_master_rm;
pthread_mutex_t ivat_master_update_mut;
int ivat_master_mouse_grab;
int ivat_master_shift_enabled;
/*******************/


static void ivat_master_setup() {
  ivat_master_active_connections = 0;

  ivat_master_alive = 1;
  ivat_master_scale = 0.01;
  ivat_master_mouse_grab = 0;
  ivat_master_shift_enabled = 0;
  ivat_master_spall_angle = 10;
  math_vec_set(ivat_master_in_hit, 0, 0, 0);
  math_vec_set(ivat_master_out_hit, 0, 0, 0);

  math_vec_set(ivat_master_camera_pos, 1, 1, 1);
  math_vec_set(ivat_master_shot_pos, 0, 0, 0);

  ivat_master_camera_foc = ivat_master_camera_pos;
  ivat_master_camera_foc.v[1] += 1;
  ivat_master_azim = 90;
  ivat_master_elev = 0;
  ivat_master_rm = RENDER_METHOD_PHONG;

  frame_ind[0] = 0;
  frame_ind[1] = 0;
  frame_cur_ind = 0;
  frame_ind_done = 0;

  rgb_frame[0] = NULL;
  rgb_frame[1] = NULL;

  rgb_frame[0] = malloc(3 * db.env.img_w * db.env.img_h);
  rgb_frame[1] = malloc(3 * db.env.img_w * db.env.img_h);

  memset(rgb_frame[0], 0, 3 * db.env.img_w * db.env.img_h);
  memset(rgb_frame[1], 0, 3 * db.env.img_w * db.env.img_h);
}


void ivat_master(int port, int obs_port, char *proj, char *list, char *exec, char *comp_host) {
  int frame_num, app_size;
  void *app_data;
  struct timeval start, cur;


  /* Parse Env Data */
  common_db_load(&db, proj);

  /* Setup defaults */
  ivat_master_setup();

  /* Initialize Python Processor */
  ivat_python_init();

  /* Mutex for everytime the master builds update data to send to nodes */
  pthread_mutex_init(&ivat_master_update_mut, 0);

  /* Initialize tienet master */
  ivat_master_tile_num = (db.env.img_w * db.env.img_h) / (db.env.tile_w * db.env.tile_h);
  tienet_master_init(port, ivat_master_result, list, exec, 5, IVAT_VER_KEY);

  /* Launch a thread to handle networking */
  pthread_create(&ivat_master_networking_thread, NULL, ivat_master_networking,&obs_port);

  /* Connect to the component Server */
  ivat_compnet_connect(comp_host, IVAT_COMPNET_PORT);

  /* Parse and pack the application data */
  printf("loading scene... ");
  fflush(stdout);
  app_size = common_pack(&db, &app_data, proj);
  printf("done.\n");

  tienet_master_prep(app_data, app_size);

  /* Initialize the work dispatcher */
  ivat_dispatcher_init();

  /* Data for computing fps */
  frame_num = 0;
  gettimeofday(&start, NULL);

  while(ivat_master_alive) {
    /* Update Camera Position */
    ivat_master_update();

    /* Fill the work buffer */
    ivat_dispatcher_generate(&db, ivat_master_slave_data, ivat_master_slave_data_len);

    frame_cur_ind = 1 - frame_cur_ind;

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
  ivat_dispatcher_free();

  free(rgb_frame[0]);
  free(rgb_frame[1]);

  /* End the networking thread */
  pthread_join(ivat_master_networking_thread, NULL);

  ivat_python_free();
}


void ivat_master_result(void *res_buf, int res_len) {
  ivat_master_socket_t *sock;
  common_work_t work;
  unsigned char *rgb_data;
  int i, ind, num;
  short frame;
  char c, name[256];


  /* Work unit data */
  memcpy(&work, res_buf, sizeof(common_work_t));

  if(work.size_x == 0) {  /* ugly hack until I get a better identifier in place */
    char *mesg;
    short slop;

    /* Read and advance to data */
    ind = sizeof(common_work_t);

    /* first hit */
    memcpy(&ivat_master_in_hit, &((unsigned char *)res_buf)[ind], sizeof(TIE_3));
    ind += sizeof(TIE_3);

    /* last hit */
    memcpy(&ivat_master_out_hit, &((unsigned char *)res_buf)[ind], sizeof(TIE_3));
    ind += sizeof(TIE_3);

    /* Set the Center of of Rotation */
    math_vec_add(ivat_master_cor, ivat_master_in_hit, ivat_master_out_hit);
    math_vec_mul_scalar(ivat_master_cor, ivat_master_cor, 0.5);

    /* number of meshes */
    memcpy(&num, &((unsigned char *)res_buf)[ind], sizeof(int));
    ind += sizeof(int);

    /* reset the component server to collapse all fields */
    ivat_compnet_reset();

    for(i = 0; i < num; i++) {
      memcpy(&c, &((unsigned char *)res_buf)[ind], 1);
      ind += 1;
      memcpy(name, &((unsigned char *)res_buf)[ind], c);
      ind += c;
      ivat_compnet_update(name, 1);
    }

    /* Send a message to update hitlist */
    mesg = malloc(sizeof(short) + res_len - sizeof(common_work_t) - 6*sizeof(tfloat));

    ind = 0;
    slop = IVAT_OP_SHOT;
    memcpy(&((char *)mesg)[ind], &slop, sizeof(short));
    ind += sizeof(short);

    memcpy(&((char *)mesg)[ind], &((unsigned char *)res_buf)[sizeof(common_work_t) + 6*sizeof(tfloat)], res_len - sizeof(common_work_t) - 6*sizeof(tfloat));
    ind += res_len - sizeof(common_work_t) - 6*sizeof(tfloat);

    tienet_master_broadcast(mesg, ind);
    free(mesg);
  } else {
    /* Pointer to RGB Data */
    rgb_data = &((unsigned char *)res_buf)[sizeof(common_work_t)];

    /* Frame index */
    memcpy(&frame, &((char *)res_buf)[sizeof(common_work_t) + 3 * work.size_x * work.size_y], sizeof(short));

    frame_ind[frame]++;

    /* Copy the tile into the image */
    ind = 0;
    for(i = work.orig_y; i < work.orig_y + work.size_y; i++) {
      memcpy(&((char *)rgb_frame[frame])[3 * (work.orig_x + i * db.env.img_w)], &rgb_data[ind], 3*work.size_y);
      ind += 3*work.size_y;
    }


    /* Image is complete, draw the frame. */
    if(frame_ind[frame] == ivat_master_tile_num) {
      frame_ind[frame] = 0;
      frame_ind_done = frame;

      for(sock = ivat_master_socklist; sock; sock = sock->next) {
        if(sock->next) {
          if(!sock->frame_sem.val)
            tienet_sem_post(&(sock->frame_sem));
        }
      }
    }
  }
}


void* ivat_master_networking(void *ptr) {
  ivat_master_socket_t *sock, *tmp;
  struct sockaddr_in master, observer;
  fd_set readfds;
  int port, master_socket, highest_fd, new_socket, error;
  unsigned int addrlen;
  unsigned char op;
  short endian;
#if IVAT_USE_COMPRESSION
  void *comp_buf;
#endif


  port = *(int *)ptr;


#if IVAT_USE_COMPRESSION
  comp_buf = malloc(3 * db.env.img_w * db.env.img_h + 1024);
#endif

  /* create a socket */
  if((master_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "unable to create socket, exiting.\n");
    exit(1);
  }

  /* initialize socket list */
  ivat_master_socklist = (ivat_master_socket_t *)malloc(sizeof(ivat_master_socket_t));
  ivat_master_socklist->next = NULL;
  ivat_master_socklist->prev = NULL;
  ivat_master_socklist->num = master_socket;

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
  listen(ivat_master_socklist->num, 3);

  addrlen = sizeof(observer);
  ivat_master_active_connections = 0;

  while(ivat_master_alive || ivat_master_active_connections) {
    /* wait for some network activity */
    select(highest_fd+1, &readfds, NULL, NULL, NULL);
    /* cycle through each socket and address the activity */
    for(sock = ivat_master_socklist; sock; sock = sock->next) {
      /* if no longer alive then mark each of the active connections as no longer active */
      if(!ivat_master_alive)
        sock->active = 0;

      /* check if activity was on this socket */
      if(FD_ISSET(sock->num, &readfds)) {
        if(sock->num == master_socket) {
          /* new connection */
          new_socket = accept(master_socket, (struct sockaddr *)&observer, &addrlen);
          if(new_socket >= 0) {
            tmp = ivat_master_socklist;
            ivat_master_socklist = (ivat_master_socket_t *)malloc(sizeof(ivat_master_socket_t));
            ivat_master_socklist->num = new_socket;
            ivat_master_socklist->controller = ivat_master_active_connections ? 0 : 1;
            ivat_master_socklist->active = 1;
            ivat_master_socklist->next = tmp;
            ivat_master_socklist->prev = NULL;
            tienet_sem_init(&(ivat_master_socklist->frame_sem), 0);
            tmp->prev = ivat_master_socklist;
            if(new_socket > highest_fd)
              highest_fd = new_socket;
            ivat_master_active_connections++;
          }
        } else {
          op = 255;
          /* observer communication */
          error = tienet_recv(sock->num, &op, 1, 0);

          /* remove socket from pool if there's an error, i.e slave disconnected */
          if(error || op == IVAT_NET_OP_QUIT || !sock->active) {
            op = IVAT_NET_OP_QUIT;
            tienet_send(sock->num, &op, 1, 0);

            tmp = sock;
            if(sock->prev)
              sock->prev->next = sock->next;
            /* master is always last, no need to check for sock->next next */
            sock->next->prev = sock->prev;
            if(sock == ivat_master_socklist)
              ivat_master_socklist = ivat_master_socklist->next;
            close(sock->num);
            sock = sock->next;
            free(tmp);
            ivat_master_active_connections--;

          } else {
            switch(op) {
              case IVAT_NET_OP_INIT:
                /* Send screen width and height */
                endian = 1;
                tienet_send(sock->num, &endian, sizeof(short), 0);
                tienet_send(sock->num, &db.env.img_w, sizeof(int), 0);
                tienet_send(sock->num, &db.env.img_h, sizeof(int), 0);
                break;

              case IVAT_NET_OP_FRAME:
                tienet_sem_wait(&(sock->frame_sem));

                /* Let observer know everything is okay, continue as usual. */
                op = IVAT_NET_OP_NOP;
                tienet_send(sock->num, &op, 1, 0);

                {
                  SDL_Event event_queue[64];
                  short event_num;

                  /* Get the event Queue and process it */
                  tienet_recv(sock->num, &event_num, sizeof(short), 0);
                  if(event_num)
                    tienet_recv(sock->num, event_queue, event_num * sizeof(SDL_Event), 0);
                  ivat_master_process_events(event_queue, event_num, sock);
                }

#if IVAT_USE_COMPRESSION
                {
                  unsigned long dest_len;
                  int comp_size;
                  dest_len = 3 * db.env.img_w * db.env.img_h + 1024;

                  /* frame data */
                  compress(&((char *)comp_buf)[sizeof(int)], &dest_len, rgb_frame[frame_ind_done], 3 * db.env.img_w * db.env.img_h);
                  comp_size = dest_len;
                  memcpy(comp_buf, &comp_size, sizeof(int));
                  /* int for frame size in bytes followed by actual rgb frame data */
                  tienet_send(sock->num, comp_buf, comp_size + sizeof(int), 0);
                }
#else
                /* frame data */
                tienet_send(sock->num, rgb_frame[frame_ind_done], db.env.img_w * db.env.img_h * 3, 0);
#endif

                /* Send overlay data */
                {
                  ivat_overlay_data_t overlay;

                  overlay.camera_pos = ivat_master_camera_pos;
                  overlay.azimuth = ivat_master_azim;
                  overlay.elevation = ivat_master_elev;
                  sprintf(overlay.resolution, "%dx%d", db.env.img_w, db.env.img_h);
                  overlay.controller = sock->controller;

                  tienet_send(sock->num, &overlay, sizeof(ivat_overlay_data_t), 0);
                }

                /* Lock things down so that ivat_master_update data doesn't get tainted */
                pthread_mutex_lock(&ivat_master_update_mut);

                pthread_mutex_unlock(&ivat_master_update_mut);
                break;

              case IVAT_NET_OP_MESG:
                {
                  char *string;
                  int len;

                  string = (char *)malloc(1024);
                  tienet_recv(sock->num, &len, 1, 0);
                  tienet_recv(sock->num, string, len, 0);

                  ivat_python_code(string);
                  len = strlen(string) + 1;

                  tienet_send(sock->num, &len, 1, 0);
                  tienet_send(sock->num, string, len, 0);

                  free(string);
                }
                break;


              case IVAT_NET_OP_QUIT:
                ivat_master_active_connections = 0;
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
    for(sock = ivat_master_socklist; sock; sock = sock->next) {
      if(sock->num > highest_fd)
        highest_fd = sock->num;
      FD_SET(sock->num, &readfds);
    }
  }

#if IVAT_USE_COMPRESSION
  free(comp_buf);
#endif


  /* free ivat_master_socklist */
  for(sock = ivat_master_socklist->next; sock; sock = sock->next)
    free(sock->prev);

  return 0;
}


/* additional baggage that goes with each work unit */
void ivat_master_update() {
  char op;

  ivat_master_slave_data_len = 0;

  /* Lock things down so that ivat_master_update data doesn't get tainted */
  pthread_mutex_lock(&ivat_master_update_mut);

  /* function */
  op = IVAT_OP_RENDER;
  memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], &op, 1);
  ivat_master_slave_data_len += 1;

  /* Frame Index */
  memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], &frame_cur_ind, sizeof(short));
  ivat_master_slave_data_len += sizeof(short);

  /* Camera Position */
  memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], ivat_master_camera_pos.v, sizeof(TIE_3));
  ivat_master_slave_data_len += sizeof(TIE_3);

  /* Camera Focus */
  memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], ivat_master_camera_foc.v, sizeof(TIE_3));
  ivat_master_slave_data_len += sizeof(TIE_3);

  /* Rendering Method and Data */
  memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], &ivat_master_rm, 1);
  ivat_master_slave_data_len += 1;

  switch(ivat_master_rm) {
    case RENDER_METHOD_PLANE:
      memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], &ivat_master_shot_pos, sizeof(TIE_3));
      ivat_master_slave_data_len += sizeof(TIE_3);

      memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], &ivat_master_shot_dir, sizeof(TIE_3));
      ivat_master_slave_data_len += sizeof(TIE_3);
      break;

    case RENDER_METHOD_SPALL:
      memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], &ivat_master_in_hit, sizeof(TIE_3));
      ivat_master_slave_data_len += sizeof(TIE_3);

      memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], &ivat_master_shot_dir, sizeof(TIE_3));
      ivat_master_slave_data_len += sizeof(TIE_3);

      memcpy(&((char *)ivat_master_slave_data)[ivat_master_slave_data_len], &ivat_master_spall_angle, sizeof(tfloat));
      ivat_master_slave_data_len += sizeof(tfloat);
      break;

    default:
      break;
  }

  pthread_mutex_unlock(&ivat_master_update_mut);
}


void ivat_master_process_events(SDL_Event *event_queue, int event_num, ivat_master_socket_t *sock) {
  int i, update;
  TIE_3 vec, vec2, vec3;
  tfloat celev;


  for(i = 0; i < event_num; i++) {
    update = 0;
    switch(event_queue[i].type) {
      printf("event_type: %d\n", event_queue[i].type);
      case SDL_KEYDOWN:
        update = 1;
        switch(event_queue[i].key.keysym.sym) {
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
            ivat_master_shift_enabled = 1;
            break;

          case SDLK_0: /* RENDER_METHOD_PATH */
            ivat_master_rm = RENDER_METHOD_PATH;
            break;

          case SDLK_1: /* RENDER_METHOD_NORMAL */
            ivat_master_rm = RENDER_METHOD_NORMAL;
            break;

          case SDLK_2: /* RENDER_METHOD_PHONG */
            ivat_master_rm = RENDER_METHOD_PHONG;
            break;

          case SDLK_3: /* RENDER_METHOD_PLANE */
            ivat_master_rm = RENDER_METHOD_PLANE;
            break;

          case SDLK_4: /* RENDER_METHOD_COMPONENT */
            ivat_master_rm = RENDER_METHOD_COMPONENT;
            break;

          case SDLK_5: /* RENDER_METHOD_SPALL */
            ivat_master_rm = RENDER_METHOD_SPALL;
            break;

          case SDLK_9: /* RENDER_METHOD_GRID */
            ivat_master_rm = RENDER_METHOD_GRID;
            break;

          case SDLK_UP:
            math_vec_sub(vec, ivat_master_camera_foc, ivat_master_camera_pos);
            math_vec_mul_scalar(vec, vec, ivat_master_scale*10.0);
            math_vec_add(ivat_master_camera_pos, ivat_master_camera_pos, vec);
            break;

          case SDLK_DOWN:
            math_vec_sub(vec, ivat_master_camera_foc, ivat_master_camera_pos);
            math_vec_mul_scalar(vec, vec, ivat_master_scale*10.0);
            math_vec_sub(ivat_master_camera_pos, ivat_master_camera_pos, vec);
            break;

          case SDLK_LEFT:
            /* strafe left */
            math_vec_sub(vec, ivat_master_camera_foc, ivat_master_camera_pos);
            math_vec_set(vec2, 0, 0, 1);
            math_vec_cross(vec3, vec2, vec);
            math_vec_mul_scalar(vec3, vec3, (ivat_master_scale*10.0));
            math_vec_add(ivat_master_camera_pos, ivat_master_camera_pos, vec3);
            break;

          case SDLK_RIGHT:
            /* strafe right */
            math_vec_sub(vec, ivat_master_camera_foc, ivat_master_camera_pos);
            math_vec_set(vec2, 0, 0, 1);
            math_vec_cross(vec3, vec2, vec);
            math_vec_mul_scalar(vec3, vec3, (ivat_master_scale*10.0));
            math_vec_sub(ivat_master_camera_pos, ivat_master_camera_pos, vec3);
            break;

          case SDLK_F12: /* Server Shutdown and quit*/
            ivat_master_alive = 0;
            break;

          case SDLK_e: /* export frame */
#if 0
            pthread_mutex_lock(&ivat_observer_gui_mut);
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
            pthread_mutex_unlock(&ivat_observer_gui_mut);
#endif
            break;

          case SDLK_f: /* fullscreen toggle */
            /* Reserved for Fullscreen */
            break;

	  case SDLK_g: /* grab mouse */
            ivat_master_mouse_grab = ivat_master_mouse_grab ^ 1;
	    break;

          case SDLK_q: /* quit */
            sock->active = 0;
            break;

          case SDLK_KP1: /* front, back */
            {
              tfloat dist;

              /* distance to center of rotation */
              math_vec_sub(vec, ivat_master_camera_pos, ivat_master_cor);
              math_vec_dot(dist, vec, vec);
              dist = sqrt(dist);

              ivat_master_camera_pos = ivat_master_cor;
              if(ivat_master_shift_enabled) {
                ivat_master_camera_pos.v[1] += dist;
                ivat_master_azim = 270;
                ivat_master_elev = 0;
              } else {
                ivat_master_camera_pos.v[1] -= dist;
                ivat_master_azim = 90;
                ivat_master_elev = 0;
              }
            }
            break;

          case SDLK_KP3: /* right, left */
            {
              tfloat dist;

              /* distance to center of rotation */
              math_vec_sub(vec, ivat_master_camera_pos, ivat_master_cor);
              math_vec_dot(dist, vec, vec);
              dist = sqrt(dist);

              ivat_master_camera_pos = ivat_master_cor;
              if(ivat_master_shift_enabled) {
                ivat_master_camera_pos.v[0] -= dist;
                ivat_master_azim = 0;
                ivat_master_elev = 0;
              } else {
                ivat_master_camera_pos.v[0] += dist;
                ivat_master_azim = 180;
                ivat_master_elev = 0;
              }
            }
            break;

          case SDLK_KP7: /* top, bottom */
            {
              tfloat dist;

              /* distance to center of rotation */
              math_vec_sub(vec, ivat_master_camera_pos, ivat_master_cor);
              math_vec_dot(dist, vec, vec);
              dist = sqrt(dist);

              ivat_master_camera_pos = ivat_master_cor;
              if(ivat_master_shift_enabled) {
                ivat_master_camera_pos.v[2] -= dist;
                ivat_master_azim = 0;
                ivat_master_elev = 90;
              } else {
                ivat_master_camera_pos.v[2] += dist;
                ivat_master_azim = 0;
                ivat_master_elev = -90;
              }
            }
            break;

          case SDLK_KP0: /* set camera position and direction to shot position and direction */
            ivat_master_camera_pos = ivat_master_shot_pos;
            /* project and unitize shot vector onto xy plane */
            vec = ivat_master_shot_dir;
            vec.v[2] = 0;
            math_vec_unitize(vec);

            ivat_master_azim = vec.v[1] < 0 ? 360.0 - acos(vec.v[0])*math_rad2deg : acos(vec.v[0])*math_rad2deg;
            ivat_master_elev = asin(ivat_master_shot_dir.v[2]) * math_rad2deg;
            break;


          case SDLK_KP_DIVIDE: /* shotline */
            {
              void *mesg;
              common_work_t work;
              TIE_3 dir;
              int dlen;
              char op;

              /* Queue a work unit for a shot needed for the plane render method */
              mesg = malloc(sizeof(common_work_t) + 2 * sizeof(TIE_3));
              dlen = 0;

              work.orig_x = 0;
              work.orig_y = 0;
              work.size_x = 0;
              work.size_y = 0;

              memcpy(&((char *)mesg)[dlen], &work, sizeof(common_work_t));
              dlen += sizeof(common_work_t);

              /* function */
              op = IVAT_OP_SHOT;
              memcpy(&((char *)mesg)[dlen], &op, 1);
              dlen += 1;

              /* position */
              memcpy(&((char *)mesg)[dlen], &ivat_master_camera_pos, sizeof(TIE_3));
              dlen += sizeof(TIE_3);

              math_vec_sub(dir, ivat_master_camera_foc, ivat_master_camera_pos);
              math_vec_unitize(dir);

              /* direction */
              memcpy(&((char *)mesg)[dlen], &dir, sizeof(TIE_3));
              dlen += sizeof(TIE_3);

              ivat_master_shot_pos = ivat_master_camera_pos;
              ivat_master_shot_dir = dir;

              tienet_master_push(mesg, dlen);
              free(mesg);
            }
            break;

          case SDLK_KP_MULTIPLY: /* spawl cone */
            {
              void *mesg;
              common_work_t work;
              TIE_3 dir;
              int dlen;
              char op;

              /* Queue a work unit for a shot needed for the plane render method */
              mesg = malloc(sizeof(common_work_t) + 2 * sizeof(TIE_3) + sizeof(tfloat));
              dlen = 0;

              work.orig_x = 0;
              work.orig_y = 0;
              work.size_x = 0;
              work.size_y = 0;

              memcpy(&((char *)mesg)[dlen], &work, sizeof(common_work_t));
              dlen += sizeof(common_work_t);

              /* function */
              op = IVAT_OP_SPALL;
              memcpy(&((char *)mesg)[dlen], &op, 1);
              dlen += 1;

              /* position */
              memcpy(&((char *)mesg)[dlen], &ivat_master_camera_pos, sizeof(TIE_3));
              dlen += sizeof(TIE_3);

              math_vec_sub(dir, ivat_master_camera_foc, ivat_master_camera_pos);
              math_vec_unitize(dir);

              /* direction */
              memcpy(&((char *)mesg)[dlen], &dir, sizeof(TIE_3));
              dlen += sizeof(TIE_3);

              /* angle */
              memcpy(&((char *)mesg)[dlen], &ivat_master_spall_angle, sizeof(tfloat));
              dlen += sizeof(tfloat);

              ivat_master_shot_pos = ivat_master_camera_pos;
              ivat_master_shot_dir = dir;

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
        switch(event_queue[i].key.keysym.sym) {
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
            ivat_master_shift_enabled = 0;
            break;

          default:
            break;
        }

      case SDL_MOUSEBUTTONDOWN:
        update = 1;
        if(event_queue[i].button.button == SDL_BUTTON_WHEELUP)
          ivat_master_scale *= 1.25;

        if(event_queue[i].button.button == SDL_BUTTON_WHEELDOWN)
          ivat_master_scale *= 0.8;
        break;

      case SDL_MOUSEMOTION:
        if(event_queue[i].motion.state && ivat_master_mouse_grab) {
	  int dx, dy;

	  dx = -event_queue[i].motion.xrel;
	  dy = -event_queue[i].motion.yrel;

          update = 1;
          if(event_queue[i].button.button & 1<<(SDL_BUTTON_LEFT-1)) {
            /* backward and forward */
            math_vec_sub(vec, ivat_master_camera_foc, ivat_master_camera_pos);
            math_vec_mul_scalar(vec, vec, (ivat_master_scale*dy));
            math_vec_add(ivat_master_camera_pos, ivat_master_camera_pos, vec);

            /* strafe */
#if 0
            math_vec_sub(vec, ivat_master_camera_foc, ivat_master_camera_pos);
            vec2.v[0] = 0;
            vec2.v[1] = 0;
            vec2.v[2] = 1;
            math_vec_cross(vec3, vec2, vec);
            math_vec_mul_scalar(vec3, vec3, (ivat_master_scale*dx));
            math_vec_add(ivat_master_camera_pos, ivat_master_camera_pos, vec3);
#endif
          } else if(event_queue[i].button.button & 1<<(SDL_BUTTON_RIGHT-1)) {
            /* if the shift key is held down then rotate about Center of Rotation */
            if(ivat_master_shift_enabled) {
              TIE_3 vec;
              tfloat radius, angle;

              vec.v[0] = ivat_master_cor.v[0] - ivat_master_camera_pos.v[0];
              vec.v[1] = ivat_master_cor.v[1] - ivat_master_camera_pos.v[1];
              vec.v[2] = 0;

              angle = vec.v[0]*vec.v[0] + vec.v[1]*vec.v[1];
              radius = sqrt(angle);
              vec.v[0] /= radius;
              vec.v[1] /= radius;

              vec.v[0] = vec.v[1] < 0 ? 360.0 - acos(vec.v[0])*math_rad2deg : acos(vec.v[0])*math_rad2deg;

              vec.v[0] -= 0.035*dx;
              vec.v[0] *= math_deg2rad;

              ivat_master_camera_pos.v[0] = -radius*cos(vec.v[0]) + ivat_master_cor.v[0];
              ivat_master_camera_pos.v[1] = -radius*sin(vec.v[0]) + ivat_master_cor.v[1];
              ivat_master_azim -= 0.035*dx;
            } else {
              ivat_master_azim += 0.035*dx;
              ivat_master_elev += 0.035*dy;
            }
          } else if(event_queue[i].button.button & 1<<(SDL_BUTTON_MIDDLE-1)) {
            ivat_master_camera_pos.v[2] += ivat_master_scale*dy;

            /* strafe */
            math_vec_sub(vec, ivat_master_camera_foc, ivat_master_camera_pos);
            math_vec_set(vec2, 0, 0, 1);
            math_vec_cross(vec3, vec2, vec);
            math_vec_mul_scalar(vec3, vec3, (ivat_master_scale*dx));
            math_vec_add(ivat_master_camera_pos, ivat_master_camera_pos, vec3);
          }

          ivat_master_azim = fmod(ivat_master_azim, 360.0);
          ivat_master_elev = fmod(ivat_master_elev, 360.0);
        }
        break;

      default:
        break;
    }

    if(update) {
      ivat_master_camera_foc = ivat_master_camera_pos;
      celev = cos(ivat_master_elev * math_deg2rad);
      ivat_master_camera_foc.v[0] += cos(ivat_master_azim * math_deg2rad) * celev;
      ivat_master_camera_foc.v[1] += sin(ivat_master_azim * math_deg2rad) * celev;
      ivat_master_camera_foc.v[2] += sin(ivat_master_elev * math_deg2rad);
    }
  }
}
