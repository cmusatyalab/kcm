#ifndef _DCM_AVAHI_H_
#define _DCM_AVAHI_H_

#include <sys/param.h>
#include <glib.h>

#define KCM_MAX_SERVICES 16


typedef struct {
  AvahiServiceBrowser *kab_browser;
  unsigned short       kab_port;
  char                *kab_hostname;
} kcm_avahi_browse_t;


typedef struct {
  AvahiEntryGroup     *kap_group;
  unsigned short       kap_port;
  char                *kap_service_name;
} kcm_avahi_publish_t;


typedef struct {
  GMainLoop           *kai_loop;
  pthread_mutex_t      kai_mut;
  AvahiPoll           *kai_gpoll_api;
  AvahiGLibPoll       *kai_gpoll;
  AvahiClient         *kai_client;
  char                *kai_service_name;
  kcm_avahi_browse_t  *kai_browse;
  kcm_avahi_publish_t  kai_services[KCM_MAX_SERVICES];
} kcm_avahi_internals_t;


int kcm_avahi_init(GMainLoop *loop);
int kcm_avahi_browse(char *service_name, int if_index);
int kcm_avahi_publish(char *service_name, int if_index, unsigned short port);
int kcm_avahi_unpublish(char *service_name);


#endif /*_DCM_AVAHI_H_ */
