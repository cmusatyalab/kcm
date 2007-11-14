#ifndef _COMMON_H_
#define _COMMON_H_

ssize_t readn(int fd, void *vptr, size_t n); 
ssize_t writen(int fd, const void *vptr, size_t n); 
void tunnel(int localsock, int remotesock);
unsigned short choose_random_port(void);

#endif /* _COMMON_H_ */
