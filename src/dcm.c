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

#include "common.h"
#include "dcm.h"
#include "dcm-avahi.h"
#include "dcm-glib.h"
#include "dcm-dbus-glue.h"

G_DEFINE_TYPE(DCM, dcm, G_TYPE_OBJECT);

GMainLoop *main_loop = NULL;

static void
dcm_class_init(DCMClass *klass) {
  GError *error = NULL;

  fprintf(stderr, "(dcm) initializing DCM class..\n");

  /* Connect to the system D-Bus, which allows us to connect to
   * service discovery mechanisms like Avahi and BlueZ. */

  fprintf(stderr, "(dcm) getting bus..\n");

  klass->conn = dbus_g_bus_get(DBUS_BUS_SESSION, &error);
  if(klass->conn == NULL) {
    g_warning("(dcm) failed to open connection to bus: %s\n", error->message);
    g_error_free(error);
    return;
  }

  fprintf(stderr, "(dcm) installing object type info..\n");

  /* &dbus_glib_dcm_object_info is provided in the dcm-dbus-glue.h file */
  dbus_g_object_type_install_info(dcm_get_type(), &dbus_glib_dcm_object_info);

  return;
}


static void
dcm_init(DCM *server) {
  GError *error = NULL;
  DBusGProxy *driver_proxy;
  DCMClass *klass;
  guint32 request_ret;


  fprintf(stderr, "(dcm) initializing DCM object..\n");

  /* Get our connection to the D-Bus. */
  klass = G_TYPE_INSTANCE_GET_CLASS(server, dcm_get_type(), DCMClass);
  if(klass == NULL) {
    fprintf(stderr, "(dcm) couldn't find our DCM class instance!\n");
    return;
  }
  if(klass->conn == NULL)
    fprintf(stderr, "(dcm) couldn't find our DCM class DBus connection!\n");


  fprintf(stderr, "(dcm) associating DCM GObject with service path= %s..\n", 
	  DCM_DBUS_SERVICE_PATH);
  
  /* Register a service path with this GObject.  Must be done after installing
   * the class info above. */
  dbus_g_connection_register_g_object(klass->conn,
				      DCM_DBUS_SERVICE_PATH,
				      G_OBJECT(server));

  fprintf(stderr, "(dcm) creating DCM proxy on bus=%s..\n", 
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
    fprintf(stderr, "(dcm) failed creating DCM proxy on bus=%s!\n", 
	    DBUS_SERVICE_DBUS);
  

  /* Finally, request the DBus refer to this proxy when services
   * ask for our name. */

  fprintf(stderr, "(dcm) requesting DBus name for DCM proxy..\n");

  if(!org_freedesktop_DBus_request_name (driver_proxy, DCM_DBUS_SERVICE_NAME,
					 0, &request_ret, &error))	{
    fprintf(stderr, "(dcm) unable to request name on DBUS(%s)..\n", 
	    DCM_DBUS_SERVICE_NAME);
    g_warning("Unable to register service: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (driver_proxy);
}

 
gboolean
dcm_client(DCM *server, guint *gport, GError **error) {
  volatile int port;
  pthread_t tid;

  fprintf(stderr, "(dcm) Received client call. \n");


  /* We should browse for services here now that we know exactly which
   * service we're performing. */

  if(avahi_client_main(main_loop) < 0) {
    fprintf(stderr, "(dcm) Error connecting to Avahi!\n");
    return FALSE;
  }
  
  /* Here, we create the thread that will establish and tunnel between
   * local and remote connections, found in "client.c". */
  
  port = 0;
  if(pthread_create(&tid, NULL, client_main, (void *)&port) != 0) {
    fprintf(stderr, "(dcm) Error creating server thread!\n");
    return FALSE;
  }

  fprintf(stderr, "(dcm) Waiting for child thread to choose port..\n");
  while(port == 0) continue;

  *gport = port;

  fprintf(stderr, "(dcm) Returning from client() call!\n");

  return TRUE;
}


gboolean
dcm_server(DCM *server, guint gport, GError **error) {
  server_data *sdp;
  int listenfd;


  fprintf(stderr, "(dcm) Handling server(callback port=%d) call, "
	  "starting Avahi!\n", gport);

  /* We should register services here now that we know exactly which
   * service we're performing. */
  listenfd = avahi_server_main(main_loop, "DCMServer");
  if(listenfd < 0) {
    fprintf(stderr, "(dcm) Error registering with Avahi!\n");
    return FALSE;
  }
  
  fprintf(stderr, "(dcm) Avahi started.  Creating tunneling thread..\n");


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
    fprintf(stderr, "(dcm) error creating server thread!\n");
    return FALSE;
  }

  fprintf(stderr, "(dcm) Returning from server() call!\n");

  return TRUE;
}


int 
main(int argc, char *argv[]) {
  DCM *server;

  fprintf(stderr, "(dcm) starting up..\n");
  bzero((void *)&remote_host, sizeof(host_t));
  
  g_type_init();
  
  fprintf(stderr, "(dcm) creating new dcm object..\n");

  server = g_object_new(dcm_get_type(), NULL);
  
  fprintf(stderr, "(dcm) creating new gmain loop..\n");

  main_loop = g_main_loop_new(NULL, FALSE);

  g_main_loop_run(main_loop);

  fprintf(stderr, "(dcm) Finished main loop! Dying..\n");

  /* Clean up */
  g_main_loop_unref(main_loop);

  return 0;
}
