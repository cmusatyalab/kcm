#ifndef _DCM_AVAHI_H_
#define _DCM_AVAHI_H_

#include <sys/param.h>
#include <glib.h>

typedef struct {
  unsigned short port;
  char hostname[MAXPATHLEN];
} host_t;

extern volatile host_t remote_host;

int avahi_client_main(GMainLoop *loop, char *sname);
int avahi_server_main(GMainLoop *loop, char *sname, unsigned short port);

#endif /*_DCM_AVAHI_H_ */
