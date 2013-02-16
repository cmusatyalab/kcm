/*
 *  Kimberley Display Control Manager
 *
 *  Copyright (c) 2007-2008 Carnegie Mellon University
 *  All rights reserved.
 *
 *  Kimberley is free software: you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  Kimberley is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Kimberley. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _KCM_H_
#define _KCM_H_

#define KCM_DBUS_SERVICE_NAME "edu.cmu.cs.kimberley.kcm"
#define KCM_DBUS_SERVICE_PATH "/edu/cmu/cs/kimberley/kcm"
#define KCM_AVAHI_SERVICE_NAME "edu.cmu.cs.kimberley.kcm"

#define MAXCONN 8

typedef enum {
  CONN_INTERNET = 1,
  CONN_BLUETOOTH
} kcm_conn_type_t;

typedef struct {
  pthread_t    tid;
  unsigned int local_port;
  int          listenfd;
} server_data;


void       *client_main     (void *arg);
void       *server_main     (void *arg);

#endif /*_KCM_H_ */
