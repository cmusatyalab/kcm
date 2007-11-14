#include <stdio.h>
#include <stdlib.h>
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

#include <avahi-client/client.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "common.h"
#include "dcm.h"
#include "dcm-glib.h"
#include "dcm-dbus-glue.h"

G_DEFINE_TYPE(DCM, dcm, G_TYPE_OBJECT);


AvahiClient *avahi_client;


/* Callback for state changes in the Avahi client */
static void
avahi_client_callback(AVAHI_GCC_UNUSED AvahiClient *client, AvahiClientState state, void *userdata)
{
  GMainLoop *loop = userdata;
  
  g_message ("Avahi Client State Change: %d", state);
  
  if(state == AVAHI_CLIENT_FAILURE) {
    /* We we're disconnected from the Daemon */
    g_message("Disconnected from the Avahi Daemon: %s", 
	      avahi_strerror(avahi_client_errno(client)));
    
    /* Quit the application */
    g_main_loop_quit (loop);
  }
}


int
connect_to_avahi(GMainLoop *loop) {
  const AvahiPoll *poll_api;
  AvahiGLibPoll *glib_poll;
  const char *version;
  int error;
  
  /* Optional: Tell avahi to use g_malloc and g_free */
  avahi_set_allocator(avahi_glib_allocator());
  
  /* Create the GLib Adapter */
  glib_poll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
  poll_api = avahi_glib_poll_get(glib_poll);
  
  
  /* Create a new AvahiClient instance */
  if((avahi_client = avahi_client_new(poll_api, 0, avahi_client_callback,
				      loop, &error)) == NULL) {
    g_warning ("Error initializing Avahi: %s", avahi_strerror (error));
    goto fail;
  }


  /* Make a call to get the version string from the daemon */
  if((version = avahi_client_get_version_string(avahi_client)) == NULL) {
    g_warning("Error getting version string: %s", 
	      avahi_strerror(avahi_client_errno(avahi_client)));
    goto fail;
  }
         
  g_message ("Avahi Server Version: %s", version);

  return 0;
  
 fail:
  avahi_client_free(avahi_client);
  avahi_client = NULL;
  avahi_glib_poll_free(glib_poll);
  
  return -1;
}

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
	  DCM_SERVICE_PATH);
  
  /* Register a service path with this GObject.  Must be done after installing
   * the class info above. */
  dbus_g_connection_register_g_object(klass->conn,
				      DCM_SERVICE_PATH,
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

  if(!org_freedesktop_DBus_request_name (driver_proxy, DCM_SERVICE_NAME, 0, 
					 &request_ret, &error))	{
    fprintf(stderr, "(dcm) unable to request name on DBUS(%s)..\n", 
	    DCM_SERVICE_NAME);
    g_warning("Unable to register service: %s", error->message);
    g_error_free (error);
  }

  g_object_unref (driver_proxy);
}


 
gboolean
dcm_client(DCM *server, guint *gport, GError **error) {
  volatile int port;
  pthread_t tid;

  fprintf(stderr, "(dcm) Received client call!\n");

  /* We should browse for services here now that we know exactly which
   * service we're performing. */
  
  /* Here, we create the thread that will establish and tunnel between
   * local and remote connections, found in "client.c". */

  port = 0;
  if(pthread_create(&tid, NULL, client_main, (void *)&port) != 0) {
    fprintf(stderr, "(dcm) error creating server thread!\n");
    return FALSE;
  }

  fprintf(stderr, "(dcm) Waiting for nonzero port at %p..\n", &port);

  while(port == 0) continue;

  *gport = port;

  fprintf(stderr, "(dcm) Returning from client() call!\n");

  return TRUE;
}

gboolean
dcm_server(DCM *server, guint gport, GError **error) {
  server_data *sdp;


  /* We should register services here now that we know exactly which
   * service we're performing. */

  

  /* Receiving thread must free() */

  sdp = (server_data *)calloc(1, sizeof(server_data));
  if(sdp == NULL) {
    perror("calloc");
    return FALSE;
  }

  
  fprintf(stderr, "(dcm) Received server(port=%d) call!\n", gport);

  /* Here, we create the thread that will establish and tunnel between
   * local and remote connections, found in "server.c". */

  sdp->port = gport;
  if(pthread_create(&(sdp->tid), NULL, server_main, (void *)sdp) != 0) {
    fprintf(stderr, "(dcm) error creating server thread!\n");
    return FALSE;
  }

  fprintf(stderr, "(dcm) Returning from server() call!\n");

  return TRUE;
}


int 
main(int argc, char *argv[]) {
  GMainLoop *main_loop;
  DCM *server;

  fprintf(stderr, "(dcm) starting up..\n");
  
  g_type_init();
  
  fprintf(stderr, "(dcm) creating new dcm object..\n");

  server = g_object_new(dcm_get_type(), NULL);
  
  fprintf(stderr, "(dcm) creating new gmain loop..\n");

  main_loop = g_main_loop_new(NULL, FALSE);

  fprintf(stderr, "(dcm) connecting to Avahi..\n");

  if(connect_to_avahi(main_loop) < 0) {
    fprintf(stderr, "Could not connect to Avahi over D-Bus!\n");
    return -1;
  }
  
  fprintf(stderr, "(dcm) Connected to Avahi! Starting GLib main loop..\n");

  g_main_loop_run(main_loop);

  fprintf(stderr, "(dcm) Finished main loop! Dying..\n");

  /* Clean up */
  g_main_loop_unref(main_loop);

  return 0;
}
