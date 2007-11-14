#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"
#include "dcm.h"



/*
 * dcm_server is a call which asks the DCM to set up and make a connection
 */

/* If we're running, that means D-Bus received a message for us and
 * executed this server-side binary.  We need to:
 *   1) connect to D-Bus and receive the message that awoke us, 
 *      which contains the local port to connect on
 *   3) connect to the waiting Sun RPC server
 *   4) connect to a local service discovery mechanism (Avahi, Bluez, etc.)
 *   5) accept an incoming connection
 *   6) tunnel between our connections
 */

/* At the moment, we only support Avahi (local Internet subnet connections)
 * for service discovery.  Bluetooth is planned for the future (BlueZ). */

int
make_local_connection(unsigned short port) {
  int sockfd, err;
  char port_str[NI_MAXSERV];
  struct addrinfo *info, hints;

  
  /* Create new loopback connection to the Sun RPC server on the
   * port that it indicated in the D-Bus message. */
  
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return FALSE;
  }
    
  bzero(&hints,  sizeof(struct addrinfo));
  hints.ai_flags = AI_CANONNAME;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  snprintf(port_str, 6, "%u", port);
    
  if((err = getaddrinfo("localhost", port_str, &hints, &info)) < 0) {
    fprintf(stderr, "(dcm) getaddrinfo failed: %s\n", gai_strerror(err));
    return FALSE;
  }

  if(connect(sockfd, info->ai_addr, sizeof(struct sockaddr_in)) < 0) {
    perror("connect");
    return FALSE;
  }

  freeaddrinfo(info);

  return TRUE;
}

void *server_main(void *args) {
  int connfd;
  unsigned short port;

  if(args == NULL) {
    fprintf(stderr, "(dcm-server) bad args\n");
    pthread_exit((void *)-1);
  }
  port = *(unsigned short *)args;


  fprintf(stderr, "(dcm-server) New thread starting..\n");

  fprintf(stderr, "(dcm-server) Registering with remote services..\n");

  fprintf(stderr, "(dcm-server) Making local connection on port %u..\n", port);

  connfd = make_local_connection(port);
  if(connfd < 0) {
    pthread_exit((void *)-1);
  }

  fprintf(stderr, "(dcm-server) Accepting incoming connection..\n");

  fprintf(stderr, "(dcm-server) Tunneling between the two threads..\n");

  pthread_exit(0);
}
