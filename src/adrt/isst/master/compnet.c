#include "compnet.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include "tienet.h"

int isst_master_compserv_socket;
int isst_master_compserv_active;


#define LIST_BASE_ATTS		0
#define LIST_DEP_ATTS		1
#define LIST_ALL_ATTS		2
#define LIST_METRICS		3
#define GET_BASE_ATTS_STATE	4
#define GET_ATTS_STATE		5
#define GET_ATT_STATE		6
#define SET_BASE_ATTS_01	7
#define SET_BASE_ATTS_STATE	8
#define RESET_BASE_ATTS		9
#define	TERM			128


/*
* Establish a connection to the component server.
*/
void isst_compnet_connect(char *host, int port) {
  struct hostent hostent;
  struct sockaddr_in compserv, master;

  isst_master_compserv_active = 0;

  /* If no host name is supplied then do nothing */
  if(!strlen(host))
    return;

  /* server address */
  if(gethostbyname(host)) {
    hostent = gethostbyname(host)[0];
  } else {
    fprintf(stderr, "hostname %s unknown, exiting.\n", host);
    exit(1);
  }

  /* create a socket */
  if((isst_master_compserv_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "cannot create socket for component server connection, exiting.");
    exit(1);
  }

  /* client address */
  master.sin_family = AF_INET;
  master.sin_addr.s_addr = INADDR_ANY;
  master.sin_port = htons(0);

  compserv.sin_family = hostent.h_addrtype;
  memcpy((char*)&compserv.sin_addr.s_addr, hostent.h_addr_list[0], hostent.h_length);
  compserv.sin_port = htons(port);

  if(bind(isst_master_compserv_socket, (struct sockaddr *)&master, sizeof(master)) < 0) {
    fprintf(stderr, "unable to bind component server connection socket, exiting.\n");
    exit(1);
  }

  /* connect to master */
  if(connect(isst_master_compserv_socket, (struct sockaddr *)&compserv, sizeof(compserv)) < 0) {
    fprintf(stderr, "cannot connect to component server, exiting.\n");
    exit(1);
  }

  /* data may now be transmitted to the server */
  isst_master_compserv_active = 1;
}

/*
* Update the status of a component
*/
void isst_compnet_update(char *string, char status) {
  char message[256];

  if(!isst_master_compserv_active)
    return;

  /* format message */
  sprintf(message, "%c%s,%d%c", SET_BASE_ATTS_STATE, string, status, TERM);

  /* Send string */
  tienet_send(isst_master_compserv_socket, message, strlen(message), 0);
}


void isst_compnet_reset() {
  char message;

  if(!isst_master_compserv_active)
    return;

  message = RESET_BASE_ATTS;
  tienet_send(isst_master_compserv_socket, &message, 1, 0);
}
