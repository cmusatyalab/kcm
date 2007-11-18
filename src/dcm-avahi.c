#include <stdio.h>
#include <stdlib.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "dcm-avahi.h"

#define PORTNUM 34129

static AvahiEntryGroup *group = NULL;
static const AvahiPoll *avahi_gpoll_api;
static AvahiGLibPoll *avahi_gpoll = NULL;
static AvahiServiceBrowser *sb = NULL;
static AvahiClient *avahi_client = NULL;
static char *service_name = "DCMServer";
static int client_running = 0;
static int services_registered = 0;
static void create_services(AvahiClient *c);

char *remote_hostname = NULL;
int  port = 0;

static void 
resolve_callback(AvahiServiceResolver *r,
		 AVAHI_GCC_UNUSED AvahiIfIndex interface,
		 AVAHI_GCC_UNUSED AvahiProtocol protocol,
		 AvahiResolverEvent event,
		 const char *name,
		 const char *type,
		 const char *domain,
		 const char *host_name,
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
    fprintf(stderr, "(dcm-avahi) Failed to resolve service '%s' of type '%s' "
	    " in domain '%s': %s\n", name, type, domain, 
	    avahi_strerror(avahi_client_errno(
			      avahi_service_resolver_get_client(r))));
    break;

  case AVAHI_RESOLVER_FOUND: 
    {
      char a[AVAHI_ADDRESS_STR_MAX], *t;
      
      fprintf(stderr, "Resolved service '%s' of type '%s' in domain '%s':\n", 
	      name, type, domain);
      
      avahi_address_snprint(a, sizeof(a), address);
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
	      host_name, port, a,
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
    
    fprintf(stderr, "(dcm-avahi) %s\n", 
	    avahi_strerror(avahi_client_errno(
					avahi_service_browser_get_client(b))));
    //    avahi_simple_poll_quit(simple_poll);
    return;

  case AVAHI_BROWSER_NEW:
    fprintf(stderr, "(dcm-avahi) Found service '%s' of type '%s' in "
	    "domain '%s'\n", name, type, domain);

    /* We ignore the returned resolver object. In the callback
       function we free it. If the server is terminated before
       the callback function is called the server will free
       the resolver for us. */

    if (!(avahi_service_resolver_new(c, interface, protocol, name, type, 
				     domain, AVAHI_PROTO_UNSPEC, 0, 
				     resolve_callback, c)))
      fprintf(stderr, "(dcm-avahi) Failed to resolve service '%s': %s\n", 
	      name, avahi_strerror(avahi_client_errno(c)));
             
    break;
 
  case AVAHI_BROWSER_REMOVE:
    fprintf(stderr, "(dcm-avahi) Lost service '%s' of type '%s' in domain"
	    " '%s'\n", name, type, domain);
    break;

  case AVAHI_BROWSER_ALL_FOR_NOW:
    fprintf(stderr, "(dcm-avahi) Received ALL_FOR_NOW browser callback\n");
    break;

  case AVAHI_BROWSER_CACHE_EXHAUSTED:
    fprintf(stderr, "(dcm-avahi) Received CACHE_EXHAUSTED browser callback\n");
    break;
  }
}


/* Callback for state changes in the Avahi client */
static void
avahi_client_callback(AVAHI_GCC_UNUSED AvahiClient *client, AvahiClientState state, void *userdata)
{
  GMainLoop *loop = userdata;
  
  g_message ("(dcm-avahi) Avahi Client State Change: %d", state);
  
  switch(state) {

  case AVAHI_CLIENT_S_RUNNING:
    client_running = 1;
    break;

  case AVAHI_CLIENT_FAILURE:
    /* We're disconnected from the Daemon */
    g_message("(dcm-avahi) Disconnected from the Avahi Daemon: %s", 
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

    if (group)
      avahi_entry_group_reset(group);

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
  assert(g == group || group == NULL);

  switch (state) {

  case AVAHI_ENTRY_GROUP_ESTABLISHED :
    /* The entry group has been established successfully */
    fprintf(stderr, "(dcm-avahi) Service '%s' successfully established.\n", 
	    service_name);
    services_registered = 1;
    break;

  case AVAHI_ENTRY_GROUP_COLLISION : {
    char *n;
             
    /* A service name collision happened. Let's pick a new name */
    n = avahi_alternative_service_name(service_name);
    avahi_free(service_name);
    service_name = n;
    
    fprintf(stderr, "(dcm-avahi) Service name collision, renaming service "
	    "to '%s'\n", service_name);
    
    /* And recreate the services */
    create_services(avahi_entry_group_get_client(g));
    break;
  }

  case AVAHI_ENTRY_GROUP_FAILURE :
    
    fprintf(stderr, "(dcm-avahi) Entry group failure: %s\n",
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
create_services(AvahiClient *c) {
  int ret;

  assert(c);


  /* If this is the first time we're called, let's create a new entry group */

  if(group == NULL) {
    group = avahi_entry_group_new(c, avahi_entry_group_callback, NULL);
    if(group == NULL) {
      fprintf(stderr, "(dcm-avahi) Failed creating new entry group: %s\n", 
	      avahi_strerror(avahi_client_errno(c)));
      goto fail;
    }
  }


  /* Add our service */

  fprintf(stderr, "(dcm-avahi) Adding service '%s'\n", service_name);

  if((ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC, 
					  AVAHI_PROTO_UNSPEC, 0, service_name, 
					  "_dcm._tcp", NULL, NULL, 
					  PORTNUM, NULL)) < 0) {
    fprintf(stderr, "(dcm-avahi) Failed to add _dcm._tcp service: %s\n",
	    avahi_strerror(ret));
    goto fail;
  }
 

  /* Tell the Avahi server to register the service */

  fprintf(stderr, "(dcm-avahi) Committing service '%s'\n", service_name);

  if((ret = avahi_entry_group_commit(group)) < 0) {
    fprintf(stderr, "(dcm-avahi) Failed to commit entry_group: %s\n",
	    avahi_strerror(ret));
    goto fail;
  }

  return;
 
 fail:
  avahi_client_free(c);
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
avahi_server_main(GMainLoop *loop, char *sname) {

  if(sname == NULL) {
    fprintf(stderr, "(dcm-avahi) No service name specified!\n");
    return -1;
  }
  service_name = sname;

  /* Allocate main loop objects and client. */

  if(!client_running)
    if(connect_to_avahi(loop) < 0) {
      fprintf(stderr, "(dcm-avahi) Failed to create glib polling object "
	      "+ api.\n");
      goto fail;
    }

  fprintf(stderr, "(dcm-avahi) Waiting for Avahi client to come up..\n");

  while(!client_running)
    continue;


  /* Avahi has started up successfully, so it's time to create our services */
  
  fprintf(stderr, "(dcm-avahi) Registering services with Avahi..\n");

  if(!group)
    create_services(avahi_client);

  fprintf(stderr, "(dcm-avahi) Waiting for entry group callbacks..\n");

  while(!services_registered)
    continue;

  fprintf(stderr, "(dcm-avahi) Done with Avahi registration!\n");
     
  return 0;

 fail:

  /* Cleanup things */

  if(avahi_client) {
    avahi_client_free(avahi_client);
    avahi_client = NULL;
  }

  //  if (simple_poll)
  //    avahi_simple_poll_free(simple_poll);

  avahi_free(service_name);
  service_name = NULL;

  return -1;
}


int 
avahi_client_main(GMainLoop *loop) {

  /* Allocate main loop objects and client. */

  fprintf(stderr, "(dcm-avahi) Connecting to Avahi..\n");

  if(!client_running) {
    if(connect_to_avahi(loop) < 0) {
      fprintf(stderr, "(dcm-avahi) Failed to create glib polling object "
	      "+ api.\n");
      goto fail;
    }
  }

  fprintf(stderr, "(dcm-avahi) Waiting for Avahi client to come up..\n");

  while(!client_running)
    continue;


  fprintf(stderr, "(dcm-avahi) Creating service browser.. \n");
  
  /* Create the service browser */

  if(sb == NULL) {
    sb = avahi_service_browser_new(avahi_client, AVAHI_IF_UNSPEC,
				   AVAHI_PROTO_UNSPEC, "_dcm._tcp", NULL, 0,
				   browse_callback, avahi_client);
    if(sb == NULL) {
      fprintf(stderr, "(dcm-avahi) Failed to create service browser: %s\n", 
	      avahi_strerror(avahi_client_errno(avahi_client)));
      goto fail;
    }
  }

  fprintf(stderr, "(dcm-avahi) Waiting for browser and resolver callbacks "
	  "with connection info.\n");

  while(remote_hostname == NULL)
    continue;

  return 0;

 fail:

  /* Cleanup things */

  if(sb != NULL) {
    avahi_service_browser_free(sb);
    sb = NULL;
  }

  if(avahi_client != NULL) {
    avahi_client_free(avahi_client);
    avahi_client = NULL;
  }

  //  if(simple_poll)
  //  avahi_simple_poll_free(simple_poll);

  return -1;
}
