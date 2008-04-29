#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <glib.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "avahi.h"
#include "common.h"
#include "kcm.h"
#include "kcm-glib.h"
#include "kcm-dbus-glue.h"

G_DEFINE_TYPE(KCM, kcm, G_TYPE_OBJECT);

GMainLoop *main_loop = NULL;
SSL_CTX *ctx = NULL;

static void
kcm_class_init(KCMClass *klass) {
  GError *error = NULL;

  fprintf(stderr, "(kcm) initializing KCM class..\n");

  /* Connect to the system D-Bus, which allows us to connect to
   * service discovery mechanisms like Avahi and BlueZ. */

  fprintf(stderr, "(kcm) getting bus..\n");

  klass->conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
  if(klass->conn == NULL) {
    g_warning("(kcm) failed to open connection to bus: %s\n", error->message);
    g_error_free(error);
    return;
  }

  fprintf(stderr, "(kcm) installing object type info..\n");

  /* &dbus_glib_kcm_object_info is provided in the kcm-dbus-glue.h file */
  dbus_g_object_type_install_info(kcm_get_type(), &dbus_glib_kcm_object_info);

  return;
}


static void
kcm_init(KCM *server) {
  GError *error = NULL;
  DBusGProxy *driver_proxy;
  KCMClass *klass;
  guint32 request_ret;


  fprintf(stderr, "(kcm) initializing KCM object..\n");

  /* Get our connection to the D-Bus. */
  klass = G_TYPE_INSTANCE_GET_CLASS(server, kcm_get_type(), KCMClass);
  if(klass == NULL) {
    fprintf(stderr, "(kcm) couldn't find our KCM class instance!\n");
    return;
  }
  if(klass->conn == NULL)
    fprintf(stderr, "(kcm) couldn't find our KCM class DBus connection!\n");


  fprintf(stderr, "(kcm) associating KCM GObject with service path= %s..\n", 
	  KCM_DBUS_SERVICE_PATH);
  
  /* Register a service path with this GObject.  Must be done after installing
   * the class info above. */
  dbus_g_connection_register_g_object(klass->conn,
				      KCM_DBUS_SERVICE_PATH,
				      G_OBJECT(server));

  fprintf(stderr, "(kcm) creating KCM proxy on bus=%s..\n", 
	  DBUS_SERVICE_DBUS);

  /* Create a proxy for the interface exported by the service name.
   * After this is done, all method calls and signals over this proxy
   * will go through the DBus message bus to the name owner above.
   * The constants here are defined in dbus-glib-bindings.h */
  driver_proxy = dbus_g_proxy_new_for_name(klass->conn,
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);
  if(driver_proxy == NULL)
    fprintf(stderr, "(kcm) failed creating KCM proxy on bus=%s!\n", 
	    DBUS_SERVICE_DBUS);
  

  /* Finally, request the DBus refer to this proxy when services
   * ask for our name. */

  fprintf(stderr, "(kcm) requesting DBus name for KCM proxy..\n");

  if(!org_freedesktop_DBus_request_name (driver_proxy, KCM_DBUS_SERVICE_NAME,
					 0, &request_ret, &error))	{
    fprintf(stderr, "(kcm) unable to request name on DBUS(%s)..\n", 
	    KCM_DBUS_SERVICE_NAME);
    g_warning("Unable to register service: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (driver_proxy);
}


gboolean
kcm_sense(KCM *server, gchar **interfaces, GError **error) {
  return TRUE;
}

 
gboolean
kcm_browse(KCM *server, gchar *gname, guint *gport, GError **error) {
  volatile int port;
  pthread_t tid;

  fprintf(stderr, "(kcm) Received client call (%s). \n", gname);


  /* We should browse for services here now that we know exactly which
   * service we're performing. */

  if(avahi_client_main(main_loop, (char *)gname) < 0) {
    fprintf(stderr, "(kcm) Error connecting to Avahi!\n");
    return FALSE;
  }
  
  /* Here, we create the thread that will establish and tunnel between
   * local and remote connections, found in "client.c". */
  
  port = 0;
  if(pthread_create(&tid, NULL, client_main, (void *)&port) != 0) {
    fprintf(stderr, "(kcm) Error creating server thread!\n");
    return FALSE;
  }

  fprintf(stderr, "(kcm) Waiting for child thread to choose port..\n");
  while(port == 0) continue;

  *gport = port;

  fprintf(stderr, "(kcm) Returning from client() call!\n");

  return TRUE;
}


gboolean
kcm_publish(KCM *server, gchar *gname, guint gport, GError **error) {
  server_data *sdp;
  struct sockaddr_in saddr;
  socklen_t slen;
  unsigned short port;
  int listenfd;

  fprintf(stderr, "(kcm) Received server call (%s, %d).\n", gname, gport);

  fprintf(stderr, "(kcm-avahi) Setting up local port for Avahi services "
	  "to listen on..\n");

  listenfd = listen_on_any_tcp_port();
  if(listenfd < 0) {
    fprintf(stderr, "(kcm-avahi) Couldn't create a listening socket!\n");
    return FALSE;
  }

  slen = sizeof(struct sockaddr_in);
  if(getsockname(listenfd, (struct sockaddr *)&saddr, &slen) < 0) {
    perror("getsockname");
    return FALSE;
  }
  port = ntohs(saddr.sin_port);


  /* We should register services with Avahi now that we know exactly which
   * service we're performing. */

  if(avahi_server_main(main_loop, (char *)gname, port) < 0) {
    fprintf(stderr, "(kcm) Error registering with Avahi!\n");
    return FALSE;
  }
  
  fprintf(stderr, "(kcm) Avahi started.  Creating tunneling thread..\n");


  /* Here, we create the thread that will establish and tunnel between
   * local and remote connections, found in "server.c". */

  /* Receiving thread must free() */

  sdp = (server_data *)calloc(1, sizeof(server_data));
  if(sdp == NULL) {
    perror("calloc");
    return FALSE;
  }

  sdp->local_port = gport;
  sdp->listenfd = listenfd;
  if(pthread_create(&(sdp->tid), NULL, server_main, (void *)sdp) != 0) {
    fprintf(stderr, "(kcm) error creating server thread!\n");
    return FALSE;
  }

  fprintf(stderr, "(kcm) Returning from server() call!\n");

  return TRUE;
}

void
usage(char *argv0) {
  fprintf(stderr, "%s <SSL-certificate> <SSL-private-key>\n", argv0);
}

int 
main(int argc, char *argv[]) {
  KCM  *server;
  char *certfile, *keyfile;

  if(argc != 3) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  certfile = argv[1];
  keyfile = argv[2];
  fprintf(stderr, "(kcm) reading certificate at %s\n", certfile);
  fprintf(stderr, "(kcm) reading private key at %s\n", keyfile);

  fprintf(stderr, "(kcm) initializing SSL..\n");

  THREAD_setup();
  init_OpenSSL();
  seed_prng(1024);

  ctx = setup_ctx(certfile, keyfile);

  bzero((void *)&remote_host, sizeof(host_t));
  
  g_type_init();
  
  fprintf(stderr, "(kcm) creating new kcm object..\n");

  server = g_object_new(kcm_get_type(), NULL);
  
  fprintf(stderr, "(kcm) creating new gmain loop..\n");

  main_loop = g_main_loop_new(NULL, FALSE);

  g_main_loop_run(main_loop);

  fprintf(stderr, "(kcm) Finished main loop! Dying..\n");

  /* Clean up */
  g_main_loop_unref(main_loop);

  return 0;
}
