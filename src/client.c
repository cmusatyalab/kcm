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

#include "avahi.h"
#include "common.h"


void *
client_main(void *arg) {
  int *port = arg;
  int listenfd, local_connfd, remote_connfd;
  unsigned short sp;
  socklen_t slen;
  struct sockaddr_in saddr;
  SSL *remote_ssl;
  client_params_t *parms = arg;

  fprintf(stderr, "(dcm-client) New thread starting..\n");


  /* We should browse for services here now that we know exactly which
   * service we're performing. */

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
  parms->port = sp = ntohs(saddr.sin_port);


  /*
   * Now wait until Avahi callbacks trigger after service discovery.
   */

  while(parms->host->kci_hostname == NULL)
    continue;


  fprintf(stderr, "(dcm-client) Listening locally on port %u..\n", sp);

  fprintf(stderr, "(dcm-client) Waiting for browser and resolver callbacks "
	  "with connection info.\n");


  fprintf(stderr, "(dcm-client) Making remote connection to %s:%d..\n",
	  parms->host->kci_hostname, parms->host->kci_port);

  remote_connfd = make_tcpip_connection(parms->host->kci_hostname, 
					parms->host->kci_port);
  if(remote_connfd < 0) {
    fprintf(stderr, "(dcm-client) Couldn't make remote connection!\n");
    pthread_exit((void *)-1);
  }

  fprintf(stderr, "(dcm-client) Encrypting remote connection..\n");

  remote_ssl = SSL_new(ctx);
  if(remote_ssl == NULL) {
    fprintf(stderr, "(dcm-client) Couldn't generate SSL!\n");
    pthread_exit((void *)-1);
  }
  
  if(SSL_set_fd(remote_ssl, remote_connfd) < 0) {
    fprintf(stderr, "(dcm-client) Couldn't set SSL descriptor!\n");
    pthread_exit((void *)-1);
  }
  
  if(SSL_connect(remote_ssl) <=0) {
    fprintf(stderr, "(dcm-client) Couldn't give SSL handshake!\n");
    pthread_exit((void *)-1);
  }

  fprintf(stderr, "(dcm-client) Accepting incoming connection..\n");

  local_connfd = accept(listenfd, NULL, NULL);
  if(local_connfd < 0) {
    perror("accept");
    pthread_exit((void *)-1);
  }    
  
  fprintf(stderr, "(dcm-client) Accepted! Tunneling between the two threads..\n");

  tunnel(local_connfd, remote_connfd, remote_ssl);

  pthread_exit(0);
}
