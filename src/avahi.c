#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/address.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

#include "avahi.h"
#include "common.h"


static kcm_avahi_internals_t *kcm_avahi_state = NULL;


static void 
kcm_avahi_resolve_callback(AvahiServiceResolver *r,
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
  kcm_avahi_browse_t *kb = (kcm_avahi_browse_t *)userdata;
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
     
      if(kb != NULL) {
	avahi_address_snprint(a, sizeof(a), address);
	kb->kab_hostname = strndup(a, AVAHI_ADDRESS_STR_MAX);
	kb->kab_port = port;

	/*
	 * Send information outside of Avahi module.
	 */
	if(kb->kab_conninfo != NULL) {
	  kb->kab_conninfo->kci_port = port;
	  kb->kab_conninfo->kci_hostname = strndup(a, AVAHI_ADDRESS_STR_MAX);
	}
      }

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


/*
 * Function for handling callbacks from an Avahi service browser.
 */

static void 
kcm_avahi_browse_callback(AvahiServiceBrowser *b,
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
    return;

  case AVAHI_BROWSER_NEW:
    fprintf(stderr, "(kcm-avahi) Found service '%s' of type '%s' in "
	    "domain '%s' on interface '%d'\n", name, type, domain, interface);

    /* We ignore the returned resolver object. In the callback
     * function we free it. If the server is terminated before
     * the callback function is called the server will free
     * the resolver for us. */

    if (!(avahi_service_resolver_new(c, interface, protocol, name, type, 
				     domain, AVAHI_PROTO_UNSPEC, 0, 
				     kcm_avahi_resolve_callback, userdata)))
      fprintf(stderr, "(kcm-avahi) Failed to create service resolver: %s\n", 
	      avahi_strerror(avahi_client_errno(c)));
             
    break;
 
  case AVAHI_BROWSER_REMOVE:
    fprintf(stderr, "(kcm-avahi) Lost service '%s' of type '%s' in domain"
	    " '%s' on interface '%d'\n", name, type, domain, interface);
    break;

  case AVAHI_BROWSER_ALL_FOR_NOW:
    fprintf(stderr, "(kcm-avahi) Received ALL_FOR_NOW browser callback\n");
    break;

  case AVAHI_BROWSER_CACHE_EXHAUSTED:
    fprintf(stderr, "(kcm-avahi) Received CACHE_EXHAUSTED browser callback\n");
    break;
  }
}


/*
 * Function for handling callbacks for state changes in the Avahi client.
 */

static void
kcm_avahi_client_callback(AVAHI_GCC_UNUSED AvahiClient *client, 
		      AvahiClientState state, 
		      void *userdata)
{
  GMainLoop *loop = userdata;
  
  fprintf(stderr, "(kcm-avahi) Avahi Client State Change: %d\n", state);
  
  switch(state) {


  case AVAHI_CLIENT_S_RUNNING:

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


/*
 * Function for callbacks for state changes in a service's entry group.
 */

static void 
kcm_avahi_entry_group_callback(AvahiEntryGroup *g, 
			       AvahiEntryGroupState state, 
			       AVAHI_GCC_UNUSED void *userdata) 
{
  switch (state) {

  case AVAHI_ENTRY_GROUP_ESTABLISHED :

    /*
     * The entry group has been established successfully.
     */

    fprintf(stderr, "(kcm-avahi) Entry group successfully established.\n");

    break;


  case AVAHI_ENTRY_GROUP_COLLISION :
             
    /* A service name collision happened. Fail in our case, since the 
     * application now has no idea what name is being broadcast.  If in
     * the future the user is presented with a list of possible services,
     * then this code could be revisited to choose a new name. */
    
    fprintf(stderr, "(kcm-avahi) Entry group name collision; failure.\n");

    break;


  case AVAHI_ENTRY_GROUP_FAILURE :
    
    fprintf(stderr, "(kcm-avahi) Entry group failure: %s\n",
	    avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

    /* Some kind of failure happened while we were registering our services */
    //avahi_glib_poll_free(avahi_gpoll);
    break;
    
  case AVAHI_ENTRY_GROUP_UNCOMMITED:
  case AVAHI_ENTRY_GROUP_REGISTERING:
    ;
  }
}


int
kcm_avahi_init(GMainLoop *loop) 
{
  int err, i;
  kcm_avahi_internals_t *temp;


  if(kcm_avahi_state != NULL) {
    fprintf(stderr, "(kcm-avahi) KCM Avahi already initialized!\n");
    return -1;
  }

  if(loop == NULL) {
    fprintf(stderr, "(kcm-avahi) Bad GLib loop parameter.\n");
    return -1;
  }


  /*
   * Allocate and initialize Avahi client state.
   */

  temp = malloc(sizeof(kcm_avahi_internals_t));
  if(temp == NULL) {
    perror("malloc");
    return -1;
  }

  temp->kai_loop = loop;
  temp->kai_gpoll_api = NULL;
  temp->kai_gpoll = NULL;
  temp->kai_client = NULL;
  temp->kai_service_name = "KCM Service";

  err = pthread_mutex_init(&temp->kai_mut, NULL);
  if(err != 0) {
    fprintf(stderr, "(kcm-avahi) pthread_mutex_init failed: %d\n", err);
    goto init_fail;
  }

  err = pthread_mutex_lock(&temp->kai_mut);
  if(err != 0) {
    fprintf(stderr, "(kcm-avahi) pthread_mutex_lock failed: %d\n", err);
    goto init_fail;
  }
  

  /*
   * Tell avahi to use GLib memory allocation.
   */

  avahi_set_allocator(avahi_glib_allocator());

  
  /*
   * Create a GLib polling primitive.
   */

  temp->kai_gpoll = avahi_glib_poll_new(NULL, G_PRIORITY_DEFAULT);
  if(temp->kai_gpoll == NULL) {
    fprintf(stderr, "(kcm-avahi) avahi_glib_poll_new failed.\n");
    goto init_fail;
  }

  temp->kai_gpoll_api = avahi_glib_poll_get(temp->kai_gpoll);
  if(temp->kai_gpoll_api == NULL) {
    fprintf(stderr, "(kcm-avahi) avahi_glib_poll_get failed.\n");
    goto init_fail;
  }
  
  
  /*
   * Create a new AvahiClient instance
   */

  temp->kai_client = avahi_client_new(temp->kai_gpoll_api, 
				      0, 
				      kcm_avahi_client_callback,
				      temp, 
				      &err);
  if(temp->kai_client == NULL) {
    g_warning ("(kcm-avahi) Error initializing Avahi: %s", 
	       avahi_strerror(err));
    goto init_fail;
  }

  for(i=0; i<KCM_MAX_SERVICES; i++)
    temp->kai_services[i] = NULL;

  temp->kai_browse = NULL;

  kcm_avahi_state = temp;
  pthread_mutex_unlock(&temp->kai_mut);

  return 0;

  
 init_fail:

  if(temp != NULL) {
    if(temp->kai_client != NULL)
      avahi_client_free(temp->kai_client);
    if(temp->kai_gpoll != NULL)
      avahi_glib_poll_free(temp->kai_gpoll);
  }
  
  return -1;
}


int
kcm_avahi_publish(char *service_name, int if_index, unsigned short port)
{
  int err, i;
  AvahiEntryGroup *group;
  AvahiIfIndex iface;
  kcm_avahi_publish_t *service;

  if(service_name == NULL) {
    fprintf(stderr, "(kcm-avahi) No service name specified!\n");
    return -1;
  }

  if(kcm_avahi_state == NULL) {
    fprintf(stderr, "(kcm-avahi) Trying to publish, but KCM Avahi"
	    " not initialized!\n");
    return -1;
  }

  if(if_index >= 0)
    iface = if_index;
  else
    iface = AVAHI_IF_UNSPEC;

  fprintf(stderr, "(kcm-avahi) Registering '%s' with Avahi on port %u..\n", 
	  service_name, port);


  service = malloc(sizeof(kcm_avahi_publish_t));
  if(service == NULL) {
    perror("malloc");
    return -1;
  }

  err = pthread_mutex_lock(&kcm_avahi_state->kai_mut);
  if(err != 0) {
    fprintf(stderr, "(kcm-avahi) pthread_mutex_lock failed!\n");
    goto publish_fail;
  }


  /*
   * Perform bookkeeping of registered services.
   */

  for(i=0; i<KCM_MAX_SERVICES; i++) {
    if(kcm_avahi_state->kai_services[i] == NULL) {
      kcm_avahi_state->kai_services[i] = service;
      break;
    }
  }

  if(i >= KCM_MAX_SERVICES) {
    fprintf(stderr, "(kcm-avahi) Too many services! Not registering %s..\n",
	    service_name);
    goto publish_fail;
  }


  /*
   * We must create a new entry group for each new service, or whenever
   * a service is renamed.  TXT and port updates, however, can reuse
   * an existing entry group with the AVAHI_PUBLISH_UPDATE flag.
   */

  group = avahi_entry_group_new(kcm_avahi_state->kai_client, 
				kcm_avahi_entry_group_callback, 
				NULL);
  if(group == NULL) {
    fprintf(stderr, "(kcm-avahi) Failed creating new entry group: %s\n", 
	    avahi_strerror(avahi_client_errno(kcm_avahi_state->kai_client)));
    goto publish_fail;
  }


  service->kap_group = group;
  service->kap_service_name = strdup(service_name);
  service->kap_port = port;


  /* 
   * Add the new service to the entry group.
   */

  fprintf(stderr, "(kcm-avahi) Adding service '%s'\n", 
	  service->kap_service_name);

  err = avahi_entry_group_add_service(group,
				      iface,
				      AVAHI_PROTO_UNSPEC, 
				      0, 
				      service->kap_service_name, 
				      KCM_AVAHI_TYPE,
				      NULL, 
				      NULL, 
				      port, 
				      NULL);
  if(err < 0) {
    fprintf(stderr, "(kcm-avahi) Failed to add service: %s\n",
	    avahi_strerror(err));
    goto publish_fail;
  }
 

  /*
   * Tell the Avahi daemon to register the service.
   */

  fprintf(stderr, "(kcm-avahi) Committing service '%s'\n", 
	  service->kap_service_name);

  err = avahi_entry_group_commit(group);
  if(err < 0) {
    fprintf(stderr, "(kcm-avahi) Failed to commit entry_group: %s\n",
	    avahi_strerror(err));
    goto publish_fail;
  }


  pthread_mutex_unlock(&kcm_avahi_state->kai_mut);

  return 0;


 publish_fail:

  
  
  if(service != NULL)
    free(service);

  if(group)
    avahi_entry_group_free(group);

  return -1;
}


int 
kcm_avahi_browse(char *service_name, int if_index, kcm_avahi_connect_info_t *conninfo) 
{
  AvahiServiceBrowser *sb;
  kcm_avahi_browse_t *browser;
  int err;
  AvahiIfIndex iface;    

  if(kcm_avahi_state == NULL) {
    fprintf(stderr, "(kcm-avahi) Trying to browse, but KCM Avahi"
	    " not initialized!\n");
    return -1;
  }

  if(if_index >= 0)
    iface = if_index;
  else
    iface = AVAHI_IF_UNSPEC;  

  browser = malloc(sizeof(kcm_avahi_browse_t));
  if(browser == NULL) {
    perror("malloc");
    return -1;
  }

  browser->kab_conninfo = conninfo;

  err = pthread_mutex_lock(&kcm_avahi_state->kai_mut);
  if(err != 0) {
    fprintf(stderr, "(kcm-avahi) pthread_mutex_lock failed!\n");
    return -1;
  }

  if(kcm_avahi_state->kai_browse != NULL) {
    fprintf(stderr, "(kcm-avahi) Avahi browsing already in progress!\n");
    goto browse_fail;
  }

  kcm_avahi_state->kai_browse = browser;


  fprintf(stderr, "(kcm-avahi) Creating service browser for %s.. \n", 
	  service_name);


  /*
   * Create the service browser.
   */

  browser->kab_hostname = NULL;
  browser->kab_port = 0;

  browser->kab_browser = avahi_service_browser_new(kcm_avahi_state->kai_client,
						   iface,
						   AVAHI_PROTO_UNSPEC,
						   service_name, 
						   NULL, 
						   0,
						   kcm_avahi_browse_callback, 
						   kcm_avahi_state->kai_browse);
  if(browser->kab_browser == NULL) {
    fprintf(stderr, "(kcm-avahi) Failed to create service browser: %s\n", 
	    avahi_strerror(avahi_client_errno(kcm_avahi_state->kai_client)));
    goto browse_fail;
  }

  return 0;


 browse_fail:

  if(browser != NULL) {
    if(browser->kab_browser != NULL)
      avahi_service_browser_free(browser->kab_browser);
    free(browser);
  }

  return -1;
}
