#include "compnet.h"
#include <stdio.h>
#include <sys/socket.h>


/*
* Establish a connection to the component server.
*/
void igvt_compnet_connect(char *host, int port) {
  int compserv_socket;

  /* create a socket */
  if((compserv_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    fprintf(stderr, "unable to create Component Server socket, exiting.\n");
    exit(1);
  }


}

/*
* Update the status of a component
*/
void igvt_compnet_update(char *string, int status) {
}
