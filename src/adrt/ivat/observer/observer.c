#include "brlcad_config.h"
#include "observer.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "SDL.h"
#include "ivat.h"
#include "image.h"
#include "display.h"
#include "umath.h"
#include "ivat_struct.h"
#include "tienet.h"
#include "splash.h"
/* Networking Includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#if IVAT_USE_COMPRESSION
  #include <zlib.h>
#endif


typedef struct ivat_observer_net_info_s {
  struct hostent master;
  int port;
} ivat_observer_net_info_t;


void ivat_observer(char *host, int port);
void* ivat_observer_networking(void *ptr);
void ivat_observer_event_loop(void);


/***** GLOBALS *****/
pthread_t ivat_observer_networking_thread;
pthread_mutex_t ivat_observer_gui_mut;
pthread_mutex_t event_mut;

tienet_sem_t ivat_observer_sdlinit_sem;
tienet_sem_t ivat_observer_sdlready_sem;
tienet_sem_t ivat_observer_splash_sem;
pthread_mutex_t ivat_observer_console_mut;

int screen_w;
int screen_h;

short ivat_observer_endian;

short ivat_observer_event_queue_size;
SDL_Event ivat_observer_event_queue[64];
int ivat_observer_mouse_grab;
int ivat_observer_event_loop_alive;
int ivat_observer_display_init;
int ivat_observer_master_socket;
/*******************/



void ivat_observer(char *host, int port) {
  ivat_observer_net_info_t ni;

  /* server address */ 
  if(gethostbyname(host)) {
    ni.master = gethostbyname(host)[0];
  } else {
    fprintf(stderr, "hostname %s unknown, exiting.\n", host);
    exit(1);
  }
  ni.port = port;

  ivat_observer_event_queue_size = 0;
  ivat_observer_event_loop_alive = 1;

  pthread_mutex_init(&event_mut, 0);
  pthread_mutex_init(&ivat_observer_gui_mut, 0);

  tienet_sem_init(&ivat_observer_sdlinit_sem, 0);
  tienet_sem_init(&ivat_observer_sdlready_sem, 0);
  tienet_sem_init(&ivat_observer_splash_sem, 0);
  pthread_mutex_init(&ivat_observer_console_mut, 0);
  ivat_observer_display_init = 1;

  /* Launch a thread to handle events */
  pthread_create(&ivat_observer_networking_thread, NULL, ivat_observer_networking, &ni);

  /* Process events */
  tienet_sem_wait(&ivat_observer_sdlinit_sem);
  ivat_observer_event_loop();

  /* JOIN EVENT HANDLING THREAD */
  pthread_join(ivat_observer_networking_thread, NULL);
}


void* ivat_observer_networking(void *ptr) {
  ivat_observer_net_info_t *ni;
  struct timeval start, cur;
  struct sockaddr_in my_addr, srv_addr;
  void *frame;
  unsigned int addrlen;
  unsigned char op;
  tfloat fps;
  int frame_num;
#if IVAT_USE_COMPRESSION
  void *comp_buf;
#endif


  ni = (ivat_observer_net_info_t *)ptr;

  /* create a socket */
  if((ivat_observer_master_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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

  if(bind(ivat_observer_master_socket, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
    fprintf(stderr, "unable to bind socket, exiting.\n");
    exit(1);
  }

  /* connect to master */
  if(connect(ivat_observer_master_socket, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
    fprintf(stderr, "cannot connect to master, exiting.\n");
    exit(1);
  }

  addrlen = sizeof(srv_addr);

  /* send version and get endian info */
  op = IVAT_NET_OP_INIT;
  tienet_send(ivat_observer_master_socket, &op, 1, ivat_observer_endian);
  tienet_recv(ivat_observer_master_socket, &ivat_observer_endian, sizeof(short), 0);
  ivat_observer_endian = ivat_observer_endian == 1 ? 0 : 1;
  tienet_recv(ivat_observer_master_socket, &screen_w, sizeof(int), ivat_observer_endian);
  tienet_recv(ivat_observer_master_socket, &screen_h, sizeof(int), ivat_observer_endian);

  /* Screen size is known.  Initialize SDL and continue once it's ready */
  tienet_sem_post(&ivat_observer_sdlinit_sem);

  /* Allocate memory for frame buffer */
  frame = malloc(screen_w*screen_h*3);
#if IVAT_USE_COMPRESSION
  comp_buf = malloc(screen_w*screen_h*3);
#endif


  gettimeofday(&start, NULL);
  frame_num = 0;


  while(1) {
    /* Send request for next frame */
    op = IVAT_NET_OP_FRAME;
    tienet_send(ivat_observer_master_socket, &op, 1, ivat_observer_endian);

    /* Check whether to quit here or not */
    tienet_recv(ivat_observer_master_socket, &op, 1, ivat_observer_endian);
    if(op == IVAT_NET_OP_QUIT) {
      util_display_free();
      free(frame);
#if ivat_USE_COMPRESSION
      free(comp_buf);
#endif
      close(ivat_observer_master_socket);

      ivat_observer_event_loop_alive = 0;
      printf("Observer detatched from master.\n");
      return(NULL);
    }

    /* Send Event Queue to Master */
    pthread_mutex_lock(&event_mut);

    tienet_send(ivat_observer_master_socket, &ivat_observer_event_queue_size, sizeof(short), ivat_observer_endian);
    if(ivat_observer_event_queue_size)
      tienet_send(ivat_observer_master_socket, ivat_observer_event_queue, ivat_observer_event_queue_size * sizeof(SDL_Event), ivat_observer_endian);
    ivat_observer_event_queue_size = 0;

    pthread_mutex_unlock(&event_mut);

    /* get frame data */
#if IVAT_USE_COMPRESSION
    {
      unsigned long dest_len;
      int comp_size;

      tienet_recv(ivat_observer_master_socket, &comp_size, sizeof(int), 0);
      tienet_recv(ivat_observer_master_socket, comp_buf, comp_size, 0);

      dest_len = screen_w*screen_h*3;
      uncompress(frame, &dest_len, comp_buf, (unsigned long)comp_size);
    }
#else
    tienet_recv(ivat_observer_master_socket, frame, 3*screen_w*screen_h, 0);
#endif

    if(ivat_observer_display_init) {
      ivat_observer_display_init = 0;
      tienet_sem_post(&ivat_observer_splash_sem);
      tienet_sem_wait(&ivat_observer_sdlready_sem);
    }

    /* compute frames per second (fps) */
    frame_num++;
    if(!(frame_num % 7)) {
      gettimeofday(&cur, NULL);
      fps = (tfloat)(frame_num) / ((cur.tv_sec + (tfloat)cur.tv_usec/1000000.0) - (start.tv_sec + (tfloat)start.tv_usec/1000000.0)),
      start = cur;
      frame_num = 0;
      fflush(stdout);
    }

    /* Get the overlay data */
    {
      ivat_overlay_data_t overlay;
      char string[256];

      tienet_recv(ivat_observer_master_socket, &overlay, sizeof(ivat_overlay_data_t), 0);

      /* Wait for the console to unlock */
      pthread_mutex_lock(&ivat_observer_console_mut);

      /* Draw Frame */
      util_display_draw(frame);

      sprintf(string, "position: %.3f %.3f %.3f", overlay.camera_pos.v[0], overlay.camera_pos.v[1], overlay. camera_pos.v[2]);
      util_display_text(string, 0, 0, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);

      sprintf(string, "azim: %.3f  elev: %.3f", overlay.azimuth, overlay.elevation);
      util_display_text(string, 0, 1, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);

      sprintf(string, "fps: %.1f", fps);
      util_display_text(string, 0, 1, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_BOTTOM);

      sprintf(string, "res: %s", overlay.resolution);
      util_display_text(string, 0, 0, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_BOTTOM);

      sprintf(string, "units: meters");
      util_display_text(string, 0, 0, UTIL_JUSTIFY_RIGHT, UTIL_JUSTIFY_TOP);

      sprintf(string, "controller: %s", overlay.controller ? "yes" : "no");
      util_display_text(string, 0, 0, UTIL_JUSTIFY_RIGHT, UTIL_JUSTIFY_BOTTOM);

      util_display_cross();
      util_display_flip();

      pthread_mutex_unlock(&ivat_observer_console_mut);
    }
  }

  return(NULL);
}


void ivat_observer_process(char *content, char *response) {
  char op;

  op = IVAT_NET_OP_MESG;
  tienet_send(ivat_observer_master_socket, &op, 1, 0);

  /* length of content */
  op = strlen(content);
  if(strlen(content)) {
    op += 1;
    tienet_send(ivat_observer_master_socket, &op, 1, 0);

    /* content */
    tienet_send(ivat_observer_master_socket, content, op, 0);

    /* get the response */
    tienet_recv(ivat_observer_master_socket, &op, 1, 0);
    tienet_recv(ivat_observer_master_socket, response, op, 0);
  }
}


void ivat_observer_event_loop() {
  SDL_Event event;
  char **content_buffer, **console_buffer;
  int content_lines, console_lines, i;


  content_lines = 0;
  console_lines = 0;
  content_buffer = (char **)malloc(sizeof(char *) * 100);
  console_buffer = (char **)malloc(sizeof(char *) * 100);
  for(i = 0; i < 100; i++) {
    content_buffer[i] = (char *)malloc(80);
    console_buffer[i] = (char *)malloc(80);
  }
  content_buffer[0][0] = 0;
  console_buffer[0][0] = 0;


  /* Display Loading Splash Screen */
  util_display_init(isst_logo.width, isst_logo.height);

  SDL_WM_SetCaption("ADRT_ISST_Observer Loading...", NULL);
  util_display_draw((void *)isst_logo.pixel_data);
  util_display_flip();

  tienet_sem_wait(&ivat_observer_splash_sem);
  util_display_free();

  util_display_init(screen_w, screen_h);

  SDL_WM_SetCaption("ADRT_ISST_Observer", NULL);
  tienet_sem_post(&ivat_observer_sdlready_sem);

  while(SDL_WaitEvent(&event) >= 0 && ivat_observer_event_loop_alive) {
    switch(event.type) {
      case SDL_KEYDOWN:
        switch(event.key.keysym.sym) {
          case SDLK_f: /* fullscreen mode */
            SDL_WM_ToggleFullScreen(util_display_screen);
            break;

          case SDLK_g: /* mouse grabbing */
            ivat_observer_mouse_grab = ivat_observer_mouse_grab ^ 1;
            SDL_WM_GrabInput(ivat_observer_mouse_grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
            SDL_ShowCursor(!ivat_observer_mouse_grab);
            break;

          case SDLK_F2:
            {
              void *image24;

              /* Screen dump */
              image24 = malloc(3 * screen_w * screen_h);
              util_image_convert_32to24(image24, util_display_screen->pixels, screen_w, screen_h, 0);
              util_image_save_ppm("screenshot.ppm", image24, screen_w, screen_h);
              free(image24);
            }
            break;

          case SDLK_BACKQUOTE:
            /* Bring up the console */
            pthread_mutex_lock(&ivat_observer_console_mut);
            util_display_editor(content_buffer, &content_lines, console_buffer, &console_lines, ivat_observer_process);
            pthread_mutex_unlock(&ivat_observer_console_mut);
            break;

          default:
            break;
        }
        break;

      default:
        break;
    }

    pthread_mutex_lock(&event_mut);
    /* Build up an event queue to send prior to receiving each frame */
    if(ivat_observer_event_queue_size < 64)
      ivat_observer_event_queue[ivat_observer_event_queue_size++] = event;
    pthread_mutex_unlock(&event_mut);
  }


  for(i = 0; i < 100; i++) {
    free(content_buffer[i]);
    free(console_buffer[i]);
  }
  free(content_buffer);
  free(console_buffer);
}
