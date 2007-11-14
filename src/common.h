#ifndef _HELPER_H_
#define _HELPER_H_

#define SERVICE_NAME "edu.cmu.cs.diamond.opendiamond.dcm"
#define SERVICE_PATH "/edu/cmu/cs/diamond/opendiamond/dcm"

#define MAXCONN 8

typedef enum {
  CONN_INTERNET = 1,
  CONN_BLUETOOTH
} dcm_conn_type_t;

ssize_t readn(int fd, void *vptr, size_t n); 
ssize_t writen(int fd, const void *vptr, size_t n); 
void tunnel(int localsock, int remotesock);
unsigned short choose_random_port(void);

#endif /* _HELPER_H_ */
