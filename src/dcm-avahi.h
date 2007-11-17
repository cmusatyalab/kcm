#ifndef _DCM_AVAHI_H_
#define _DCM_AVAHI_H_

#include <glib.h>

int avahi_client_main(GMainLoop *loop);
int avahi_server_main(GMainLoop *loop);

#endif /*_DCM_AVAHI_H_ */
