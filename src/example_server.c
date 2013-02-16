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

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include "common.h"
#include "kcm-dbus-app-glue.h"

#define DBUS_SERVICE_NAME "edu.cmu.cs.kimberley.kcm"
#define DBUS_SERVICE_PATH "/edu/cmu/cs/kimberley/kcm"
#define KCM_SERVICE_NAME "_kcm._tcp"

int main(int argc, char *argv[]) {
  DBusGConnection *conn;
  DBusGProxy *proxy;
  GError *error = NULL;
  int sockfd, connfd;
  unsigned short port = 0;
  unsigned int len, i;
  gchar *name = KCM_SERVICE_NAME, **interface_strs = NULL;
  guint gport = 0;
  gint interface = -1;
  struct sockaddr_in saddr;

  fprintf(stderr, "(example-server) starting up (pid=%d)..\n", getpid());
  
  g_type_init();

  /* Create a socket and listen for loopback connections 
   * on some port number. */

  port = choose_random_port();
  
  if((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }
  
  bzero(&saddr, sizeof(struct sockaddr_in));
  saddr.sin_family = PF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  saddr.sin_port = htons(port);
  
  if(bind(sockfd, (struct sockaddr *) &saddr, 
	  sizeof(struct sockaddr_in)) < 0) {
    perror("bind");
    return -1;
  }
  
  fprintf(stderr, "(example-server) server listening on port %d..\n", port);

  if(listen(sockfd, 1) < 0) {
    perror("listen");
    return -1;
  }

  fprintf(stderr, "(example-server) connecting to DBus..\n");
  
  conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
  if(conn == NULL) {
    g_warning("Unable to connect to dbus: %sn", error->message);
    g_error_free (error);
    exit(EXIT_FAILURE);
  }

  fprintf(stderr, "(example-server) dbus proxy setting up..\n");
  
  /* This won't trigger activation! */
  proxy = dbus_g_proxy_new_for_name(conn, DBUS_SERVICE_NAME, 
				    DBUS_SERVICE_PATH, DBUS_SERVICE_NAME);
  
  fprintf(stderr, "(example-server) dbus proxy making call (sense)..\n");
  
  /* The method call will trigger activation. */
  if(!edu_cmu_cs_kimberley_kcm_sense(proxy, &interface_strs, &error)) {
    /* Method failed, the GError is set, let's warn everyone */
    g_warning("(example-server) kcm->sense() method failed: %s", 
	      error->message);
    g_error_free(error);
    exit(EXIT_FAILURE);
  }

  if(interface_strs != NULL) {
    fprintf(stderr, "(example-server) Found some interfaces:\n");
    for(i=0; interface_strs[i] != NULL; i++)
      fprintf(stderr, "\t%d: %s\n", i, interface_strs[i]);
    fprintf(stderr, "\n");
  }

  fprintf(stderr, "(example-server) dbus proxy making call (publish)..\n");

  gport = port;

  if(!edu_cmu_cs_kimberley_kcm_publish(proxy, name, interface, 
				       port, &error)) {
    if(error != NULL) {
      g_warning("kcm->publish() method failed: %s", error->message);
      g_error_free(error);
    }
    exit(EXIT_FAILURE);
  }

  len = sizeof(struct sockaddr_in);
  if((connfd = accept(sockfd, (struct sockaddr *)&saddr, &len)) < 0) {
    perror("accept");
    return -1;
  }
  
  fprintf(stderr, "(example-server) connection established!\n");

  while(TRUE)
    continue;

  /* Cleanup */
  g_object_unref (proxy);
  
  /* The DBusGConnection should never be unreffed, 
   * it lives once and is shared amongst the process */

  exit(EXIT_SUCCESS);
}
