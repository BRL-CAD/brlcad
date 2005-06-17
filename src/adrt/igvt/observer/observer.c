#include "config.h"
#include "observer.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "SDL.h"
#include "igvt.h"
#include "image.h"
#include "display.h"
#include "umath.h"
#include "struct.h"
#include "tienet.h"
/* Networking Includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#if IGVT_USE_COMPRESSION
  #include <zlib.h>
#endif


typedef struct igvt_observer_net_info_s {
  struct hostent master;
  int port;
} igvt_observer_net_info_t;


void igvt_observer(char *host, int port);
void* igvt_observer_networking(void *ptr);
void igvt_observer_event_loop(void);


/***** GLOBALS *****/
pthread_t igvt_observer_networking_thread;
pthread_mutex_t igvt_observer_gui_mut;
pthread_mutex_t event_mut;

tienet_sem_t igvt_observer_sdlinit_sem;
tienet_sem_t igvt_observer_sdlready_sem;

int screen_w;
int screen_h;

short igvt_observer_endian;

short igvt_observer_event_queue_size;
SDL_Event igvt_observer_event_queue[64];
int igvt_observer_mouse_grab;
/*******************/



void igvt_observer(char *host, int port) {
  igvt_observer_net_info_t ni;

  /* server address */ 
  if(gethostbyname(host)) {
    ni.master = gethostbyname(host)[0];
  } else {
    fprintf(stderr, "hostname %s unknown, exiting.\n", host);
    exit(1);
  }
  ni.port = port;

  igvt_observer_event_queue_size = 0;

  pthread_mutex_init(&event_mut, 0);
  pthread_mutex_init(&igvt_observer_gui_mut, 0);

  tienet_sem_init(&igvt_observer_sdlinit_sem, 0);
  tienet_sem_init(&igvt_observer_sdlready_sem, 0);

  /* Launch a thread to handle events */
  pthread_create(&igvt_observer_networking_thread, NULL, igvt_observer_networking, &ni);

  /* Process events */
  tienet_sem_wait(&igvt_observer_sdlinit_sem);
  igvt_observer_event_loop();

  /* JOIN EVENT HANDLING THREAD */
  pthread_join(igvt_observer_networking_thread, NULL);
}


void* igvt_observer_networking(void *ptr) {
  igvt_observer_net_info_t *ni;
  struct timeval start, cur;
  int sockd;
  struct sockaddr_in my_addr, srv_addr;
  void *frame;
  unsigned int addrlen;
  unsigned char op;
#if IGVT_USE_COMPRESSION
  void *comp_buf;
#endif


  ni = (igvt_observer_net_info_t *)ptr;

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
  op = IGVT_NET_OP_INIT;
  tienet_send(sockd, &op, 1, igvt_observer_endian);
  tienet_recv(sockd, &igvt_observer_endian, sizeof(short), 0);
  igvt_observer_endian = igvt_observer_endian == 1 ? 0 : 1;
  tienet_recv(sockd, &screen_w, sizeof(int), igvt_observer_endian);
  tienet_recv(sockd, &screen_h, sizeof(int), igvt_observer_endian);

  /* Screen size is known.  Initialize SDL and continue once it's ready */
  tienet_sem_post(&igvt_observer_sdlinit_sem);
  tienet_sem_wait(&igvt_observer_sdlready_sem);

  /* Allocate memory for frame buffer */
  frame = malloc(screen_w*screen_h*3);
#if IGVT_USE_COMPRESSION
  comp_buf = malloc(screen_w*screen_h*3);
#endif


  gettimeofday(&start, NULL);

  while(1) {
    /* Send request for next frame */
    op = IGVT_NET_OP_FRAME;
    tienet_send(sockd, &op, 1, igvt_observer_endian);

    /* Check whether to quit here or not */
    tienet_recv(sockd, &op, 1, igvt_observer_endian);
    if(op == IGVT_NET_OP_QUIT) {
      util_display_free();
      free(frame);
#if igvt_USE_COMPRESSION
      free(comp_buf);
#endif
      close(sockd);
      printf("Observer detatched from master.\n");
      return(NULL);
    }

    /* Send Event Queue to Master */
    pthread_mutex_lock(&event_mut);

    tienet_send(sockd, &igvt_observer_event_queue_size, sizeof(short), igvt_observer_endian);
    if(igvt_observer_event_queue_size)
      tienet_send(sockd, igvt_observer_event_queue, igvt_observer_event_queue_size * sizeof(SDL_Event), igvt_observer_endian);
    igvt_observer_event_queue_size = 0;

    pthread_mutex_unlock(&event_mut);

    /* get frame data */
#if IGVT_USE_COMPRESSION
    {
      unsigned long dest_len;
      int comp_size;

      tienet_recv(sockd, &comp_size, sizeof(int), 0);
      tienet_recv(sockd, comp_buf, comp_size, 0);

      dest_len = screen_w*screen_h*3;
      uncompress(frame, &dest_len, comp_buf, (unsigned long)comp_size);
    }
#else
    tienet_recv(sockd, frame, 3*screen_w*screen_h, 0);
#endif

    /* Draw Frame */
    util_display_draw(frame);

    /* Get the overlay data */
    {
      igvt_overlay_data_t overlay;
      char string[256];

      tienet_recv(sockd, &overlay, sizeof(igvt_overlay_data_t), 0);

      sprintf(string, "position: %.3f %.3f %.3f", overlay.camera_pos.v[0], overlay.camera_pos.v[1], overlay. camera_pos.v[2]);
      util_display_text(string, 0, 0, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);

      sprintf(string, "azim: %.3f  elev: %.3f", overlay.azimuth, overlay.elevation);
      util_display_text(string, 0, 1, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);

      sprintf(string, "res: %s", overlay.resolution);
      util_display_text(string, 0, 0, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_BOTTOM);

      sprintf(string, "units: meters");
      util_display_text(string, 0, 0, UTIL_JUSTIFY_RIGHT, UTIL_JUSTIFY_TOP);

      sprintf(string, "controller: %s", overlay.controller ? "yes" : "no");
      util_display_text(string, 0, 0, UTIL_JUSTIFY_RIGHT, UTIL_JUSTIFY_BOTTOM);
    }

    util_display_cross();
    util_display_flip();
  }

  return(NULL);
}


void igvt_observer_event_loop() {
  SDL_Event event;
  int alive = 1;


  util_display_init(screen_w, screen_h);
  SDL_WM_SetCaption("ADRT_IGVT_Observer", NULL);

  util_display_text("Loading ...", 10, 4, UTIL_JUSTIFY_LEFT, UTIL_JUSTIFY_TOP);
  util_display_flip();

  tienet_sem_post(&igvt_observer_sdlready_sem);

  SDL_EnableKeyRepeat(50, 50);

  while(SDL_WaitEvent(&event) >= 0 && alive) {
    switch(event.type) {
      case SDL_KEYDOWN:
        switch(event.key.keysym.sym) {
          case SDLK_f: /* fullscreen mode */
            SDL_WM_ToggleFullScreen(util_display_screen);
            break;

          case SDLK_g: /* mouse grabbing */
            igvt_observer_mouse_grab = igvt_observer_mouse_grab ^ 1;
            SDL_WM_GrabInput(igvt_observer_mouse_grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
            SDL_ShowCursor(!igvt_observer_mouse_grab);
            break;

          case SDLK_q: /* quit, decouple display, end this function */
            alive = 0;
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
    if(igvt_observer_event_queue_size < 64)
      igvt_observer_event_queue[igvt_observer_event_queue_size++] = event;
    pthread_mutex_unlock(&event_mut);
  }
}
