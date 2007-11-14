#include <avahi-client/client.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

static AvahiEntryGroup *group = NULL;
static const AvahiPoll *avahi_poll_api;
static AvahiGLibPoll *avahi_gpoll = NULL;
static AvahiClient *avahi_client = NULL;


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


/* Callback for state changes in the entry group */

static 
void avahi_entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) {

  assert(g == group || group == NULL);

  switch (state) {
  case AVAHI_ENTRY_GROUP_ESTABLISHED :
    /* The entry group has been established successfully */
    fprintf(stderr, "(dcm-avahi) Service '%s' successfully established.\n", 
	    name);
    break;

  case AVAHI_ENTRY_GROUP_COLLISION : {
    char *n;
             
    /* A service name collision happened. Let's pick a new name */
    n = avahi_alternative_service_name(name);
    avahi_free(name);
    name = n;
    
    fprintf(stderr, "(dcm-avahi) Service name collision, renaming service "
	    "to '%s'\n", name);
    
    /* And recreate the services */
    create_services(avahi_entry_group_get_client(g));
    break;
  }

  case AVAHI_ENTRY_GROUP_FAILURE :
     
    fprintf(stderr, "(dcm-avahi) Entry group failure: %s\n",
	    avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

    /* Some kind of failure happened while we were registering our services */
    avahi_gmain_loop_quit(avahi_gpoll);
    break;

  case AVAHI_ENTRY_GROUP_UNCOMMITED:
  case AVAHI_ENTRY_GROUP_REGISTERING:
    ;
  }
}

static void create_services(AvahiClient *c) {
  char r[128];
  int ret;

  assert(c);

  /* If this is the first time we're called, let's create a new entry group */
  if (!group)
    if (!(group = avahi_entry_group_new(c, entry_group_callback, NULL))) {
      fprintf(stderr, "avahi_entry_group_new() failed: %s\n", avahi_strerror(avahi_client_errno(c)));
      goto fail;
    }

  fprintf(stderr, "(dcm-avahi) Adding service '%s'\n", name);

  /* Create some random TXT data */
  snprintf(r, sizeof(r), "random=%i", rand());
 

  /* Add a service */
  if ((ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, name, "_printer._tcp", NULL, NULL, 515, NULL)) < 0) {
    fprintf(stderr, "Failed to add _printer._tcp service: %s\n", avahi_strerror(ret));
    goto fail;
  }
 
  /* Tell the Avahi server to register the service */
  if ((ret = avahi_entry_group_commit(group)) < 0) {
    fprintf(stderr, "Failed to commit entry_group: %s\n", avahi_strerror(ret));
    goto fail;
  }

  return;
 
 fail:
  avahi_gmain_loop_quit(avahi_gpoll);
}


int
connect_to_avahi(GMainLoop *loop) {
  const char *version;
  int error;
  
  /* Optional: Tell avahi to use g_malloc and g_free */
  avahi_set_allocator(avahi_glib_allocator());
  
  /* Create the GLib Adapter */
  avahi_gpoll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
  avahi_poll_api = avahi_glib_poll_get(avahi_gpoll);
  
  
  /* Create a new AvahiClient instance */
  if((avahi_client = avahi_client_new(poll_api, 0, avahi_client_callback,
				      loop, &error)) == NULL) {
    g_warning ("Error initializing Avahi: %s", avahi_strerror(error));
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
  avahi_glib_poll_free(avahi_gpoll);
  avahi_gpoll = NULL;
  
  return -1;
}
