/*
 *  Kimberley Display Control Manager
 *
 *  Copyright (c) 2007-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  Kimberley is free software: you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  Kimberley is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kimberley. If not, see <http://www.gnu.org/licenses/>.
 */

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

  fprintf(stderr, "(kcm-server) New thread starting..\n");

  if(args == NULL) {
    fprintf(stderr, "(kcm-server) bad args\n");
    pthread_exit((void *)-1);
  }
  data = (server_data *)args;

  fprintf(stderr, "(kcm-server) Making local connection on port %u..\n", 
	  data->local_port);

  local_connfd = make_tcpip_connection_locally(data->local_port);
  if(local_connfd < 0) {
    free(data);
    pthread_exit((void *)-1);
  }


  fprintf(stderr, "(kcm-server) Accepting remote connection requests..\n");

  remote_connfd = accept(data->listenfd, NULL, NULL);
  if(remote_connfd < 0) {
    perror("accept");
    pthread_exit((void *)-1);
  }
  
  remote_ssl = SSL_new(ctx);
  if(remote_ssl == NULL) {
    fprintf(stderr, "(kcm-server) Couldn't generate SSL!\n");
    pthread_exit((void *)-1);
  }

  if(SSL_set_fd(remote_ssl, remote_connfd) < 0) {
    fprintf(stderr, "(kcm-server) Couldn't set SSL descriptor!\n");
    pthread_exit((void *)-1);
  }

  if(SSL_accept(remote_ssl) <= 0) {
    fprintf(stderr, "(kcm-server) Couldn't accept SSL handshake!\n");
    pthread_exit((void *)-1);
  }


  fprintf(stderr, "(kcm-server) Tunneling between the two threads..\n");

  tunnel(local_connfd, remote_connfd, remote_ssl);

  pthread_exit(0);
}
