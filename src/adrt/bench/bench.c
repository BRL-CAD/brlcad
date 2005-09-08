#include "bench.h"
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
#include "unpack.h"
/*** lib util ***/
#include "camera.h"
#include "image.h"
#include "umath.h"
/*** Networking Includes ***/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>


#define LOCAL_PORT 10000


void *bench_frame;
common_db_t db;
util_camera_t camera;
tie_t tie;
void *app_data;
int app_size;
int server_socket;
pthread_t bench_thread;


static void* bench_ipc(void *ptr) {
  struct sockaddr_in server;
  struct sockaddr_in client;
  int client_socket;
  unsigned int addrlen;


  /* create a socket */
  if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "unable to create socket, exiting.\n");
    exit(1);
  }

  /* server address */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = htonl(INADDR_ANY);
  server.sin_port = htons(LOCAL_PORT);

  /* bind socket */
  if(bind(server_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "socket already bound, exiting.\n");
    exit(1);
  }

  /* listen for connections */
  listen(server_socket, 3);

  /* wait for connection before streaming data */
  addrlen = sizeof(client);
  client_socket = accept(server_socket, (struct sockaddr *)&client, &addrlen);

  /* send the application data */
printf("ipc connection established, sending data\n");
  tienet_send(client_socket, app_data, app_size, 0);
}


void bench(char *proj, int dump) {
  struct sockaddr_in server;
  struct sockaddr_in client;
  struct hostent h;
  int i, res_len, client_socket;
  char image_name[16];
  common_work_t work;
  void *mesg, *res_buf;


  printf("Loading and Prepping...\n");
  /* Camera with no threads */
  util_camera_init(&camera, 1);

  /* Parse Env Data */
  common_db_load(&db, proj);

  /* Read the data off disk and pack it */
  app_size = common_pack(&db, &app_data, proj);

  /* Launch a networking thread to do ipc data streaming */
  pthread_create(&bench_thread, NULL, bench_ipc, 0);

  /* Parse the data into memory for rendering */
  /* create a socket */
  if((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "unable to create socket, exiting.\n");
    exit(1);
  }

  /* Bind any available port number */
  client.sin_family = AF_INET;
  client.sin_addr.s_addr = htonl(INADDR_ANY);
  client.sin_port = htons(0);
 
  if(bind(client_socket, (struct sockaddr *)&client, sizeof(client)) < 0) {
    fprintf(stderr, "unable to bind socket, exiting\n");
    exit(1);
  }

  /* Establish ipc connection */
  if(gethostbyname("localhost")) {
    h = gethostbyname("localhost")[0];
  } else {
    fprintf(stderr, "unknown host: %s\n", "localhost");
    exit(1);
  }

  server.sin_family = h.h_addrtype;
  memcpy((char *)&server.sin_addr.s_addr, h.h_addr_list[0], h.h_length);
  server.sin_port = htons(LOCAL_PORT);

  if(connect(client_socket, (struct sockaddr *)&server, sizeof(server)) < 0) {
    fprintf(stderr, "cannot establish connection, exiting.\n");
    exit(1);
  }


  common_unpack(&db, &tie, &camera, client_socket);

  /* Prep */
  common_env_prep(&db.env);
  util_camera_prep(&camera, &db);

  /* Allocate memory for a frame */
  bench_frame = malloc(4 * sizeof(tfloat) * db.env.img_w * db.env.img_h);
  memset(bench_frame, 0, 4 * sizeof(tfloat) * db.env.img_w * db.env.img_h);

  /* Render a 2000x1500 (3 megapixel) image */
  work.orig_x = 0;
  work.orig_y = 0;
  work.size_x = 2000;
  work.size_y = 1500;
  mesg = malloc(sizeof(common_work_t));

  printf("Rendering...\n");
  util_camera_render(&camera, &db, &tie, &work, sizeof(common_work_t), &res_buf, &res_len);

  free(mesg);


  /* Initialize the work dispatcher */
//  rise_dispatcher_init();
//    rise_dispatcher_generate(&db, NULL, 0);

#if 0
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

  util_camera_free(&camera);
  free(app_data);
  free(bench_frame);
}
