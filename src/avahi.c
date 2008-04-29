#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "avahi.h"
#include "common.h"


static const AvahiPoll *avahi_gpoll_api = NULL;
static AvahiGLibPoll *avahi_gpoll = NULL;
static AvahiClient *avahi_client = NULL;
static char *service_name = "KCM Service";
static int client_running = 0;
static void create_services(AvahiClient *c, char *name, 
			    char *type, unsigned short port);


static void 
resolve_callback(AvahiServiceResolver *r,
		 AVAHI_GCC_UNUSED AvahiIfIndex interface,
		 AVAHI_GCC_UNUSED AvahiProtocol protocol,
		 AvahiResolverEvent event,
		 const char *name,
		 const char *type,
		 const char *domain,
		 const char *hostname,
		 const AvahiAddress *address,
		 uint16_t port,
		 AvahiStringList *txt,
		 AvahiLookupResultFlags flags,
		 AVAHI_GCC_UNUSED void* userdata)
{
 
  assert(r);
 
  /* Called whenever a service has been resolved successfully or timed out */
  
  switch (event) {

  case AVAHI_RESOLVER_FAILURE:
    fprintf(stderr, "(kcm-avahi) Failed to resolve service '%s' of type '%s' "
	    " in domain '%s': %s\n", name, type, domain, 
	    avahi_strerror(avahi_client_errno(
			      avahi_service_resolver_get_client(r))));
    break;

  case AVAHI_RESOLVER_FOUND: 
    {
      char a[AVAHI_ADDRESS_STR_MAX], *t;
      
      fprintf(stderr, "(kcm-avahi) Resolved service '%s' of type '%s' in "
	      "domain '%s':\n", name, type, domain);
     
      avahi_address_snprint(a, sizeof(a), address);
      strncpy((char *)remote_host.hostname, a, MAXPATHLEN);
      remote_host.port = port;

      t = avahi_string_list_to_string(txt);
      fprintf(stderr,
	      "\t%s:%u (%s)\n"
	      "\tTXT=%s\n"
	      "\tcookie is %u\n"
	      "\tis_local: %i\n"
	      "\tour_own: %i\n"
	      "\twide_area: %i\n"
	      "\tmulticast: %i\n"
	      "\tcached: %i\n",
	      hostname, port, a,
	      t,
	      avahi_string_list_get_service_cookie(txt),
	      !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
	      !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
	      !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
	      !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
	      !!(flags & AVAHI_LOOKUP_RESULT_CACHED));
      
      avahi_free(t);
    }
  }

  avahi_service_resolver_free(r);
}


static void 
browse_callback(AvahiServiceBrowser *b,
		AvahiIfIndex interface,
		AvahiProtocol protocol,
		AvahiBrowserEvent event,
		const char *name,
		const char *type,
		const char *domain,
		AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
		void* userdata) 
{
  AvahiClient *c = userdata;
  assert(b);

  /* Called whenever a new services becomes available on
   * the LAN or is removed from the LAN */

  switch (event) {
  case AVAHI_BROWSER_FAILURE:
    
    fprintf(stderr, "(kcm-avahi) Browser failure: %s\n", 
	    avahi_strerror(avahi_client_errno(
					avahi_service_browser_get_client(b))));
    //    avahi_simple_poll_quit(simple_poll);
    return;

  case AVAHI_BROWSER_NEW:
    fprintf(stderr, "(kcm-avahi) Found service '%s' of type '%s' in "
	    "domain '%s'\n", name, type, domain);

    /* We ignore the returned resolver object. In the callback
     * function we free it. If the server is terminated before
     * the callback function is called the server will free
     * the resolver for us. */

    if (!(avahi_service_resolver_new(c, interface, protocol, name, type, 
				     domain, AVAHI_PROTO_UNSPEC, 0, 
				     resolve_callback, c)))
      fprintf(stderr, "(kcm-avahi) Failed to create service resolver: %s\n", 
	      avahi_strerror(avahi_client_errno(c)));
             
    break;
 
  case AVAHI_BROWSER_REMOVE:
    fprintf(stderr, "(kcm-avahi) Lost service '%s' of type '%s' in domain"
	    " '%s'\n", name, type, domain);
    break;

  case AVAHI_BROWSER_ALL_FOR_NOW:
    fprintf(stderr, "(kcm-avahi) Received ALL_FOR_NOW browser callback\n");
    break;

  case AVAHI_BROWSER_CACHE_EXHAUSTED:
    fprintf(stderr, "(kcm-avahi) Received CACHE_EXHAUSTED browser callback\n");
    break;
  }
}


/* Callback for state changes in the Avahi client */

static void
avahi_client_callback(AVAHI_GCC_UNUSED AvahiClient *client, 
		      AvahiClientState state, 
		      void *userdata)
{
  GMainLoop *loop = userdata;
  
  fprintf(stderr, "(kcm-avahi) Avahi Client State Change: %d\n", state);
  
  switch(state) {


  case AVAHI_CLIENT_S_RUNNING:

    client_running = 1;
    fprintf(stderr, "(kcm-avahi) Avahi client running.\n");

    break;


  case AVAHI_CLIENT_FAILURE:

    /* We're disconnected from the Daemon */

    fprintf(stderr, "(kcm-avahi) Disconnected from the Avahi Daemon: %s", 
	    avahi_strerror(avahi_client_errno(client)));
    
    /* Quit the application */
    g_main_loop_quit (loop);

    break;

  case AVAHI_CLIENT_S_COLLISION:

    /* Let's drop our registered services. When the server is back
     * in AVAHI_SERVER_RUNNING state we will register them
     * again with the new host name. */

  case AVAHI_CLIENT_S_REGISTERING:

    /* The server records are now being established. This
     * might be caused by a host name change. We need to wait
     * for our own records to register until the host name is
     * properly esatblished. */

    //    if (group)
    //  avahi_entry_group_reset(group);

    break;

  case AVAHI_CLIENT_CONNECTING:
    ;
  }
}


/* Callback for state changes in the entry group */

static void 
avahi_entry_group_callback(AvahiEntryGroup *g, 
			   AvahiEntryGroupState state, 
			   AVAHI_GCC_UNUSED void *userdata) 
{
  switch (state) {

  case AVAHI_ENTRY_GROUP_ESTABLISHED :

    /* The entry group has been established successfully */

    fprintf(stderr, "(kcm-avahi) Entry group successfully established.\n");

    break;


  case AVAHI_ENTRY_GROUP_COLLISION :
             
    /* A service name collision happened. Fail, since the application
     * now has no idea what name is being broadcast.  */
    
    fprintf(stderr, "(kcm-avahi) Entry group name collision; failure.\n");

    break;


  case AVAHI_ENTRY_GROUP_FAILURE :
    
    fprintf(stderr, "(kcm-avahi) Entry group failure: %s\n",
	    avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

    /* Some kind of failure happened while we were registering our services */
    avahi_glib_poll_free(avahi_gpoll);
    break;
    
  case AVAHI_ENTRY_GROUP_UNCOMMITED:
  case AVAHI_ENTRY_GROUP_REGISTERING:
    ;
  }
}


static void
create_services(AvahiClient *c, char *name, char *type, unsigned short port) {
  int err;
  AvahiEntryGroup *group = NULL;

  if((c == NULL) || (name == NULL) || (type == NULL))
    return;


  /* If this is the first time we're called, let's create a new entry group */

  group = avahi_entry_group_new(c, avahi_entry_group_callback, NULL);
  if(group == NULL) {
    fprintf(stderr, "(kcm-avahi) Failed creating new entry group: %s\n", 
	    avahi_strerror(avahi_client_errno(c)));
    goto fail;
  }


  /* Add our service */

  fprintf(stderr, "(kcm-avahi) Adding service '%s'\n", type);

  err = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC, 
				      AVAHI_PROTO_UNSPEC, 0, name, 
				      type, NULL, NULL, port, NULL);
  if(err < 0) {
    fprintf(stderr, "(kcm-avahi) Failed to add service: %s\n",
	    avahi_strerror(err));
    goto fail;
  }
 

  /* Tell the Avahi server to register the service */

  fprintf(stderr, "(kcm-avahi) Committing service '%s'\n", name);

  err = avahi_entry_group_commit(group);
  if(err < 0) {
    fprintf(stderr, "(kcm-avahi) Failed to commit entry_group: %s\n",
	    avahi_strerror(err));
    goto fail;
  }

  return;
 
 fail:
  avahi_client_free(c);
  avahi_entry_group_free(group);
  avahi_glib_poll_free(avahi_gpoll);
}


static int
connect_to_avahi(GMainLoop *loop) {
  int error;
  
  /* Optional: Tell avahi to use g_malloc and g_free */
  avahi_set_allocator(avahi_glib_allocator());
  
  /* Create the GLib Adapter */
  avahi_gpoll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
  avahi_gpoll_api = avahi_glib_poll_get(avahi_gpoll);
  
  
  /* Create a new AvahiClient instance */
  if((avahi_client = avahi_client_new(avahi_gpoll_api, 0, 
				      avahi_client_callback,
				      loop, &error)) == NULL) {
    g_warning ("Error initializing Avahi: %s", avahi_strerror(error));
    goto fail;
  }

  return 0;
  
 fail:
  avahi_client_free(avahi_client);
  avahi_client = NULL;
  avahi_glib_poll_free(avahi_gpoll);
  avahi_gpoll = NULL;
  
  return -1;
}



int
avahi_server_main(GMainLoop *loop, char *name, unsigned short port) {

  if(name == NULL) {
    fprintf(stderr, "(kcm-avahi) No service name specified!\n");
    return -1;
  }


  /* Allocate main loop objects and client. */

  fprintf(stderr, "(kcm-avahi) Creating Avahi client..\n");

  if(avahi_client == NULL) {
    if(connect_to_avahi(loop) < 0) {
      fprintf(stderr, "(kcm-avahi) Failed to connect to Avahi!\n ");
      goto fail;
    }
  }

  fprintf(stderr, "(kcm-avahi) Registering %s with Avahi on port %u..\n", 
	  name, port);
  
  create_services(avahi_client, service_name, name, port);

  return 0;


 fail:

  /* Cleanup things */

  if(avahi_client) {
    avahi_client_free(avahi_client);
    avahi_client = NULL;
  }

  return -1;
}


int 
avahi_client_main(GMainLoop *loop, char *name) {
  AvahiServiceBrowser *sb = NULL;

  /* Allocate main loop objects and client. */

  fprintf(stderr, "(kcm-avahi) Connecting to Avahi..\n");

  if(avahi_client == NULL) {
    if(connect_to_avahi(loop) < 0) {
      fprintf(stderr, "(kcm-avahi) Failed to create glib polling object "
	      "+ api.\n");
      goto fail;
    }
  }

  fprintf(stderr, "(kcm-avahi) Creating service browser for %s.. \n", name);
  
  /* Create the service browser */

  remote_host.hostname[0] = '\0';
  remote_host.port = 0;

  sb = avahi_service_browser_new(avahi_client, 
				 AVAHI_IF_UNSPEC,
				 AVAHI_PROTO_UNSPEC,
				 name, NULL, 0,
				 browse_callback, 
				 avahi_client);
  if(sb == NULL) {
    fprintf(stderr, "(kcm-avahi) Failed to create service browser: %s\n", 
	    avahi_strerror(avahi_client_errno(avahi_client)));
    goto fail;
  }


  return 0;

 fail:

  /* Cleanup things */

  if(sb != NULL)
    avahi_service_browser_free(sb);

  if(avahi_client != NULL) {
    avahi_client_free(avahi_client);
    avahi_client = NULL;
  }

  return -1;
}
