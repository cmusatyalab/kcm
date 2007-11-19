#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>


/*
 * Read "n" bytes from a descriptor reliably. 
 */

ssize_t
readn(int fd, void *vptr, size_t n)
{
  size_t  nleft;
  ssize_t nread;
  char   *ptr;

  ptr = vptr;
  nleft = n;

  while (nleft > 0) {
    if ( (nread = read(fd, ptr, nleft)) < 0) {
      perror("read");
      if (errno == EINTR)
        nread = 0;      /* and call read() again */
      else
        return (-1);
    } else if (nread == 0)
      break;              /* EOF */

    nleft -= nread;
    ptr += nread;
  }
  return (n - nleft);         /* return >= 0 */
}


/*
 * Write "n" bytes to a descriptor reliably. 
 */

ssize_t                        
writen(int fd, const void *vptr, size_t n)
{
  size_t nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = vptr;
  nleft = n;
  while (nleft > 0) {
    if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0 && errno == EINTR)
        nwritten = 0;   /* and call write() again */
      else
        return (-1);    /* error */
    }

    nleft -= nwritten;
    ptr += nwritten;
  }
  return (n);
}


void
tunnel(int localsock, int remotesock) {

  while(1) {
    int size_in, size_out, in, out;
    char buf[4096];
    fd_set readfds;
    
    FD_ZERO(&readfds);
    FD_SET(localsock, &readfds);
    FD_SET(remotesock, &readfds);
    
    if(select(2, &readfds, NULL, NULL, NULL) < 0) {
      perror("select");
      exit(EXIT_FAILURE);
    }
    
    
    /* Figure out which direction information will flow. */
    
    if(FD_ISSET(localsock, &readfds))
      in = localsock, out = remotesock;
    else if(FD_ISSET(remotesock, &readfds))
      in = remotesock, out = localsock;
    else {
      fprintf(stderr, "(tunnel) select() succeeded, but no file descriptors "
	      "are ready to be read!\n");
      exit(EXIT_FAILURE);
    }
    
    
    /* Pass up to 4096 bytes of data between sockets. */
    
    size_in = read(in, (void *)buf, 4096);
    if(size_in < 0) {
      perror("read");
      continue;
    }
    else if(size_in == 0) { /* EOF */
      close(localsock); close(remotesock);
      fprintf(stderr, "(tunnel) The connection was closed.\n");
      exit(EXIT_SUCCESS);	  
    }
    
    do { 
      size_out = writen(out, (void *)buf, size_in);
      if(size_out < 0)
	perror("write");
    } while(size_out < 0);  /* Do NOT lose data. */
    
    if(size_in != size_out) {
      fprintf(stderr, "(tunnel) Somehow lost bytes, from %d in to %d out!\n", 
	      size_in, size_out);
      continue;
    }
  }
  
  return;
}


unsigned short
choose_random_port(void) {
  struct timeval t;
  unsigned short port;

  /* Choose a random TCP port between 10k and 65k to allow a client to 
   * connect on.  Use high-order bits for improved randomness. */
  
  gettimeofday(&t, NULL);
  srand(t.tv_sec);
  port = 10000 + (int)(55000.0 * (rand()/(RAND_MAX + 1.0)));

  return port;
}

static int
listen_on_tcp_port(struct sockaddr_in saddr) {
  int sockfd, bound;
  unsigned short port;

  /* Create a socket for listening. */
  
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }


  /* Choose a port at random and try binding to it. */
  
  bound = 0;
  while(!bound) {
    port = choose_random_port();
    saddr.sin_port = htons(port);
    if(bind(sockfd, (struct sockaddr *) &saddr, 
	    sizeof(struct sockaddr_in)) < 0) {
      if(errno != EADDRINUSE) {
	perror("bind");
	return -1;
      }
    }
    else bound = 1;
  }

  
  /* Success! Now make it a listening socket. */
    
  if(listen(sockfd, 1) < 0) {
    perror("listen");
    return -1;
  }

  return sockfd;
}


int
listen_on_any_tcp_port(void) {
  struct sockaddr_in saddr;
  
  bzero(&saddr, sizeof(struct sockaddr_in));
  saddr.sin_family = PF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  return listen_on_tcp_port(saddr);
}


/* Disallow remote connections as a security measure.  This also prevents us
 * from calling listen on an unbound socket.  Otherwise, we'd
 * let the system choose our port number and send it back to the client. */

int
listen_on_any_tcp_port_locally(void) {
  struct sockaddr_in saddr;

  bzero(&saddr, sizeof(struct sockaddr_in));
  saddr.sin_family = PF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  return listen_on_tcp_port(saddr);
}


int
make_tcpip_connection(char *hostname, unsigned short port) {
  int sockfd, err;
  char port_str[NI_MAXSERV];
  struct addrinfo *info, hints;

  if((hostname == NULL) || (port == 0)) 
    return -1;
  
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
    return -1;
  }
    
  bzero(&hints,  sizeof(struct addrinfo));
  hints.ai_flags = AI_CANONNAME;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  snprintf(port_str, 6, "%u", port);
    
  if((err = getaddrinfo(hostname, port_str, &hints, &info)) < 0) {
    fprintf(stderr, "(dcm) getaddrinfo failed: %s\n", gai_strerror(err));
    return -1;
  }

  if(connect(sockfd, info->ai_addr, sizeof(struct sockaddr_in)) < 0) {
    perror("connect");
    return -1;
  }

  freeaddrinfo(info);

  return 0;
}

int
make_tcpip_connection_locally(unsigned short port) {
  return make_tcpip_connection("localhost", port);
}
