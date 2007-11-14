#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"
//#include "dcm.h"
//#include "dcm-dbus-glue.h"


/* If we're running, that means D-Bus received a message for us and
 * executed this server-side binary.  We need to:
 *   1) connect to D-Bus
 *   2) receive the message that started us
 *   3) connect to a local service discovery mechanism (Avahi, Bluez, etc.)
 *   4) search for remote DCM services
 *   5) make a connection to the remote services
 *   6) listen for a local connection on a randomly chosen port
 *   7) let the client know which port by returning a D-Bus message
 *   8) accept the local connection
 *   9) tunnel between connections
 */

int
listen_on_some_local_port(int *OUT_port) {
  struct sockaddr_in saddr;
  int sockfd, bound;
  unsigned short port;

  /* Create a socket and listen for loopback connections on some port. */
  
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }
  

  /* Disallow remote connections as a security measure.  This also prevents us
   * from calling listen on an unbound socket.  Otherwise, we'd
   * let the system choose our port number and send it back to the client. */

  bzero(&saddr, sizeof(struct sockaddr_in));
  saddr.sin_family = PF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  
  bound = 0;
  while(!bound) {
    port = choose_random_port();
    saddr.sin_port = htons(port);
    if(bind(sockfd, (struct sockaddr *) &saddr, 
	    sizeof(struct sockaddr_in)) < 0) {
      if(errno != EADDRINUSE) {
	perror("bind");
	return -1;
      }
    }
    else bound = 1;
  }
    
  if(listen(sockfd, 1) < 0) {
    perror("listen");
    return -1;
  }

  *OUT_port = (int)port;

  fprintf(stderr, "(dcm-client) chose port %p=%d\n", OUT_port, port);

  return sockfd;
}


int
accept_application_connection(int sock) {
  struct sockaddr_in saddr;
  int connfd;
  unsigned int len;

  if(sock <= 0)
    return -1;

  len = sizeof(struct sockaddr_in);
  
  connfd = accept(sock, (struct sockaddr *)&saddr, &len);
  if(connfd < 0) {
    perror("accept");
    return -1;
  }
  
  return connfd;
}

void *
client_main(void *arg) {
  int *port = arg;
  int listenfd, connfd;

  fprintf(stderr, "(dcm-client) New thread starting..\n");

  fprintf(stderr, "(dcm-client) Browsing for remote services..\n");

  fprintf(stderr, "(dcm-client) Making remote connection..\n");

  fprintf(stderr, "(dcm-client) Listening locally..\n");

  listenfd = listen_on_some_local_port(port);
  if(listenfd < 0) {
    *port = -1;
    pthread_exit((void *)-1);
  }

  fprintf(stderr, "(dcm-client) Accepting incoming connection..\n");

  connfd = accept_application_connection(listenfd);
  
  fprintf(stderr, "(dcm-client) Tunneling between the two threads..\n");

  pthread_exit(0);
}
