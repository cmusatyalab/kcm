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
