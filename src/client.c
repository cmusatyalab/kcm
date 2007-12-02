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
#include "dcm-avahi.h"

volatile host_t remote_host;

void *
client_main(void *arg) {
  int *port = arg;
  int listenfd, local_connfd, remote_connfd;
  unsigned short sp;
  socklen_t slen;
  struct sockaddr_in saddr;

  fprintf(stderr, "(dcm-client) New thread starting..\n");


  listenfd = listen_on_any_tcp_port_locally();
  if(listenfd < 0) {
    *port = -1;
    pthread_exit((void *)-1);
  }

  slen = sizeof(struct sockaddr_in);
  if(getsockname(listenfd, (struct sockaddr *)&saddr, &slen) < 0) {
    perror("getsockname");
    pthread_exit((void *)-1);
  }
  *port = sp = ntohs(saddr.sin_port);

  fprintf(stderr, "(dcm-client) Listening locally on port %u..\n", sp);

  fprintf(stderr, "(dcm-client) Waiting for browser and resolver callbacks "
	  "with connection info.\n");

  while(remote_host.port == 0)
    continue;

  fprintf(stderr, "(dcm-client) Making remote connection to %s:%d..\n",
	  remote_host.hostname, remote_host.port);

  remote_connfd = make_tcpip_connection((char *)remote_host.hostname, 
					remote_host.port);
  if(remote_connfd < 0) {
    fprintf(stderr, "(dcm-client) Couldn't make remote connection!\n");
    pthread_exit((void *)-1);
  }

  fprintf(stderr, "(dcm-client) Accepting incoming connection..\n");

  local_connfd = accept(listenfd, NULL, NULL);
  if(local_connfd < 0) {
    perror("accept");
    pthread_exit((void *)-1);
  }    
  
  fprintf(stderr, "(dcm-client) Accepted! Tunneling between the two threads..\n");

  tunnel(local_connfd, remote_connfd);

  pthread_exit(0);
}
