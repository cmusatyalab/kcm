#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include "kcm-dbus-app-glue.h"

#define DBUS_SERVICE_NAME "edu.cmu.cs.kimberley.kcm"
#define DBUS_SERVICE_PATH "/edu/cmu/cs/kimberley/kcm"
#define KCM_SERVICE_NAME "_kcm._tcp"


int main(int argc, char *argv[]) {
  DBusGConnection *conn;
  DBusGProxy *proxy;
  GError *error = NULL;
  gchar *name = KCM_SERVICE_NAME, **interface_strs = NULL;
  guint port = 0, interface = -1;
  int sockfd, err, i;
  char port_str[NI_MAXSERV];
  struct addrinfo *info, hints;

  fprintf(stderr, "(example-client) starting up (pid=%d)..\n", getpid());
    
  g_type_init();

  fprintf(stderr, "(example-client) connecting to DBus session bus..\n");

  conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
  if(conn == NULL) {
    g_warning("Unable to connect to dbus: %sn", error->message);
    g_error_free (error);
    exit(EXIT_FAILURE);
  }
  
  fprintf(stderr, "(example-client) creating DBus proxy to KCM (%s)..\n",
	  DBUS_SERVICE_NAME);

  /* This won't trigger activation! */
  proxy = dbus_g_proxy_new_for_name(conn, DBUS_SERVICE_NAME, 
				    DBUS_SERVICE_PATH, DBUS_SERVICE_NAME);
  if(proxy == NULL) {
    fprintf(stderr, "(example-client) failed creating DBus proxy!\n");
    exit(EXIT_FAILURE);
  }
    
  
  fprintf(stderr, "(example-client) DBus calling into kcm (sense)..\n");

  /* The method call will trigger activation, more on that later */
  if(!edu_cmu_cs_kimberley_kcm_sense(proxy, &interface_strs, &error)) {
    /* Method failed, the GError is set, let's warn everyone */
    g_warning("(example-client) kcm->client() method failed: %s", 
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


  fprintf(stderr, "(example-client) DBus calling into kcm (browse)..\n");

  if(!edu_cmu_cs_kimberley_kcm_browse(proxy, name, interface, 
				      &port, &error)) {
    /* Method failed, the GError is set, let's warn everyone */
    g_warning("(example-client) kcm->client() method failed: %s", 
	      error->message);
    g_error_free(error);
    exit(EXIT_FAILURE);
  }

  g_print("(example-client) client() said to connect on port: %d\n", port);

  
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
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    return FALSE;
  }

  fprintf(stderr, "(example-client) connect()ing to kcm..\n");

  if(connect(sockfd, info->ai_addr, sizeof(struct sockaddr_in)) < 0) {
    perror("connect");
    return FALSE;
  }

  fprintf(stderr, "(example-client) successfully connected!\n");

  while(1)
    continue;

  freeaddrinfo(info);

  /* Cleanup */
  g_object_unref (proxy);
  
  /* The DBusGConnection should never be unreffed, 
   * it lives once and is shared amongst the process */

  exit(EXIT_SUCCESS);
}
