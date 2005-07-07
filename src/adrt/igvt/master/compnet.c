#include "compnet.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

int igvt_master_compserv_socket;

/*
* Establish a connection to the component server.
*/
void igvt_compnet_connect(char *host, int port) {
  struct hostent hostent;
  struct sockaddr_in compserv, master;

  /* server address */
  if(gethostbyname(host)) {
    hostent = gethostbyname(host)[0];
  } else {
    fprintf(stderr, "hostname %s unknown, exiting.\n", host);
    exit(1);
  }

  /* create a socket */
  if((igvt_master_compserv_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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

  if(bind(igvt_master_compserv_socket, (struct sockaddr *)&master, sizeof(master)) < 0) {
    fprintf(stderr, "unable to bind component server connection socket, exiting.\n");
    exit(1);
  }

  /* connect to master */
  if(connect(igvt_master_compserv_socket, (struct sockaddr *)&compserv, sizeof(compserv)) < 0) {
    fprintf(stderr, "cannot connect to master, exiting.\n");
    exit(1);
  }
}

/*
* Update the status of a component
*/
void igvt_compnet_update(char *string, int status) {
}
