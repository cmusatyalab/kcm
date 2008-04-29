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
#include "kcm.h"


/* At the moment, we only support Avahi (local Internet subnet connections)
 * for service discovery.  Bluetooth is planned for the future (BlueZ). */



void *server_main(void *args) {
  int local_connfd, remote_connfd;
  server_data *data;
  SSL *remote_ssl;

  fprintf(stderr, "(dcm-server) New thread starting..\n");

  if(args == NULL) {
    fprintf(stderr, "(dcm-server) bad args\n");
    pthread_exit((void *)-1);
  }
  data = (server_data *)args;

  fprintf(stderr, "(dcm-server) Making local connection on port %u..\n", 
	  data->local_port);

  local_connfd = make_tcpip_connection_locally(data->local_port);
  if(local_connfd < 0) {
    free(data);
    pthread_exit((void *)-1);
  }


  fprintf(stderr, "(dcm-server) Accepting remote connection requests..\n");

  remote_connfd = accept(data->listenfd, NULL, NULL);
  if(remote_connfd < 0) {
    perror("accept");
    pthread_exit((void *)-1);
  }
  
  remote_ssl = SSL_new(ctx);
  if(remote_ssl == NULL) {
    fprintf(stderr, "(dcm-server) Couldn't generate SSL!\n");
    pthread_exit((void *)-1);
  }

  if(SSL_set_fd(remote_ssl, remote_connfd) < 0) {
    fprintf(stderr, "(dcm-server) Couldn't set SSL descriptor!\n");
    pthread_exit((void *)-1);
  }

  if(SSL_accept(remote_ssl) <= 0) {
    fprintf(stderr, "(dcm-server) Couldn't accept SSL handshake!\n");
    pthread_exit((void *)-1);
  }


  fprintf(stderr, "(dcm-server) Tunneling between the two threads..\n");

  tunnel(local_connfd, remote_connfd, remote_ssl);

  pthread_exit(0);
}
