#ifndef _DCM_H_
#define _DCM_H_

#define DCM_DBUS_SERVICE_NAME "edu.cmu.cs.diamond.opendiamond.dcm"
#define DCM_DBUS_SERVICE_PATH "/edu/cmu/cs/diamond/opendiamond/dcm"
#define DCM_AVAHI_SERVICE_NAME "edu.cmu.cs.diamond.opendiamond.dcm"

#define MAXCONN 8

typedef enum {
  CONN_INTERNET = 1,
  CONN_BLUETOOTH
} dcm_conn_type_t;

typedef struct {
  pthread_t    tid;
  unsigned int local_port;
  int          listenfd;
} server_data;


void       *client_main     (void *arg);
void       *server_main     (void *arg);

#endif /*_DCM_H_ */
