#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "common.h"


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
tunnel(int localsock, int remotesock, SSL *remotessl) {

  if((localsock < 0) || (remotesock < 0) || (remotessl == NULL)) {
    fprintf(stderr, "(kcm-tunnel) Bad tunnel args.\n");
    return;
  }

  fprintf(stderr, "(kcm-tunnel) Tunneling between fd=%d and fd=%p\n", 
	  localsock, remotessl);

  while(1) {
    int size_in, size_out, maxfd;
    char buf[4096];
    fd_set readfds, exceptfds;
    
    FD_ZERO(&readfds);
    FD_ZERO(&exceptfds);
    FD_SET(localsock, &readfds);
    FD_SET(remotesock, &readfds);
    FD_SET(localsock, &exceptfds);
    FD_SET(remotesock, &exceptfds);

    maxfd = localsock;
    if(remotesock > maxfd)
      maxfd = remotesock;
    maxfd++;
    
    if(select(maxfd, &readfds, NULL, &exceptfds, NULL) < 0) {
      perror("select");
      close(localsock);
      close(remotesock);
      pthread_exit((void *)-1);
    }
    
    if(FD_ISSET(localsock, &exceptfds) || FD_ISSET(remotesock, &exceptfds)) {
      fprintf(stderr, "(kcm-tunnel) select() reported exceptions!\n");
      close(localsock);
      close(remotesock);
      pthread_exit((void *)-1);
    }


    /* 
     * Decide which direction information will flow.
     */

    if(FD_ISSET(localsock, &readfds)) {
    
      /* Pass up to 4096 bytes of data between sockets. */
    
      size_in = read(localsock, (void *)buf, 4096);
      if(size_in < 0) {
	perror("read");
	continue;
      }
      else if(size_in == 0) { /* EOF */
	fprintf(stderr, "(kcm-tunnel) The local connection was closed.\n");
	close(localsock);
	close(remotesock);
	pthread_exit((void *)0);
      }
    
      size_out = SSL_writen(remotessl, (void *)buf, size_in);
      if(size_out < 0) {
	fprintf(stderr, "(kcm-tunnel) Failed writing to remote connection.\n");
	close(localsock);
	close(remotesock);
	pthread_exit((void *)-1);
      }

      if(size_in != size_out) {
	fprintf(stderr, "(kcm-tunnel) Somehow lost bytes, from %d in to %d out!\n", 
		size_in, size_out);
	pthread_exit((void *)-1);
      }

      fprintf(stderr, "(kcm-tunnel) tunneled %d bytes out\n", size_in);
    }
    else if(FD_ISSET(remotesock, &readfds)) {

      reread:
	
	size_in = SSL_read_wrapper(remotessl, (void *)buf, 4096);
	if(size_in <= 0) {
          fprintf(stderr, "(kcm-tunnel) Failed on read from an SSL socket.\n");
	  pthread_exit((void *)-1);
	}
	
	if((size_out = writen(localsock, buf, size_in)) < 0) {
	  fprintf(stderr, "(kcm-tunnel) Failed to write "
		  "between file descriptors.\n");
	  pthread_exit((void *)-1);
	}
	
	if(size_in != size_out) {
	  fprintf(stderr, "(kcm-tunnel) Somehow lost bytes, from %d in to %d out!\n", 
		  size_in, size_out);
	  pthread_exit((void *)-1);
	}
	
	fprintf(stderr, "(kcm-tunnel) tunneled %d bytes in\n", size_in);


	/* If we still have SSL bytes pending, they won't be caught with
	 * the next select(), because they've been taken off the
	 * network buffer already. We have to read all SSL bytes asap. */

	if(SSL_pending(remotessl) > 0)
	  goto reread;
    }
    else {
      fprintf(stderr, "(kcm-tunnel) select() succeeded, but no file "
	      "descriptors are ready to be read!\n");
      close(localsock);
      close(remotesock);
      pthread_exit((void *)-1);
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
  saddr.sin_family = AF_INET;
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
  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  return listen_on_tcp_port(saddr);
}


int
make_tcpip_connection(char *hostname, unsigned short port)
{
  int sockfd = -1, err;
  char port_str[NI_MAXSERV];
  struct addrinfo *ai, *ret, hints = {
	.ai_family = AF_INET,
	.ai_socktype = SOCK_STREAM
  };

  if ((hostname == NULL) || (port == 0)) 
    return -1;
  
  snprintf(port_str, 6, "%u", port);
    
  err = getaddrinfo(hostname, port_str, &hints, &ret);
  if (err < 0) {
    fprintf(stderr, "(kcm) getaddrinfo failed: %s\n", gai_strerror(err));
    return -1;
  }

  for (ai = ret; ai != NULL; ai = ai->ai_next)
  {
    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd < 0) {
      perror("socket");
      continue;
    }
   
    if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) != -1)
	break;

    perror("connect");
    close(sockfd);
    sockfd = -1;
  }

  freeaddrinfo(ret);

  return sockfd;
}


int
make_tcpip_connection_locally(unsigned short port) {
  return make_tcpip_connection("localhost", port);
}


int 
init_OpenSSL(void)
{
  if(THREAD_setup() < 0) {
    fprintf(stderr, "** OpenSSL thread initialization failed!\n");
    return -1;
  }
  if(SSL_library_init() <= 0) {
    fprintf(stderr, "** OpenSSL library initialization failed!\n");
    return -1;
  }

  SSL_load_error_strings();

  return 0;
}


static void 
locking_function(int mode, int n, const char * file, int line) {
  if(mode & CRYPTO_LOCK)
    MUTEX_LOCK(mutex_buf[n]);
  else
    MUTEX_UNLOCK(mutex_buf[n]);
}


static unsigned long 
id_function(void) {
  return ((unsigned long)THREAD_ID);
}


/* This function must be called for SSL to work 
 * in a multithreaded environment. */

int 
THREAD_setup(void)
{
  int i;

  mutex_buf = (MUTEX_TYPE *)malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
  if(mutex_buf == NULL)
    return -1;

  for(i=0; i<CRYPTO_num_locks(); i++)
    MUTEX_SETUP(mutex_buf[i]);

  CRYPTO_set_id_callback(id_function);
  CRYPTO_set_locking_callback(locking_function);

  CRYPTO_set_dynlock_create_callback(dyn_create_function);
  CRYPTO_set_dynlock_lock_callback(dyn_lock_function);
  CRYPTO_set_dynlock_destroy_callback(dyn_destroy_function);
  
  return 0;
}


int 
THREAD_cleanup(void) {
  int i;

  if(mutex_buf == NULL)
    return -1;

  CRYPTO_set_id_callback(NULL);
  CRYPTO_set_locking_callback(NULL);
  
  CRYPTO_set_dynlock_create_callback(NULL);
  CRYPTO_set_dynlock_lock_callback(NULL);
  CRYPTO_set_dynlock_destroy_callback(NULL);

  for(i=0; i<CRYPTO_num_locks(); i++)
    MUTEX_CLEANUP(mutex_buf[i]);
  free(mutex_buf);
  mutex_buf = NULL;

  return 0;
}

static struct CRYPTO_dynlock_value *dyn_create_function(const char *file,
							int line) {
  struct CRYPTO_dynlock_value *value;
  
  value = (struct CRYPTO_dynlock_value *)
    malloc(sizeof(struct CRYPTO_dynlock_value));
  if(value == NULL)
    return NULL;
  
  MUTEX_SETUP(value->mutex);

  return value;
}

static void dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l,
			      const char *file, int line) {
  if(mode & CRYPTO_LOCK)
    MUTEX_LOCK(l->mutex);
  else
    MUTEX_UNLOCK(l->mutex);
}

static void dyn_destroy_function(struct CRYPTO_dynlock_value *l,
				 const char *file, int line) {
  MUTEX_CLEANUP(l->mutex);
  free(l);
}

int seed_prng(int bytes) {
  if(RAND_load_file("/dev/urandom", bytes) == 0)
    return -1;

  return 0;
}


/*
 * Read up to n bytes from an SSL descriptor.
 */

ssize_t
SSL_read_wrapper(SSL *ssl, void *vptr, size_t n)
{
  ssize_t nread; 

  if(ssl == NULL) {
    fprintf(stderr, "SSL_read_wrapper: NULL SSL\n");  
    return -1;
  }

  if(vptr == NULL) {
    fprintf(stderr, "SSL_read_wrapper: NULL dest\n");
    return -1;
  }

 retry:
  if((nread = SSL_read(ssl, vptr, n)) <= 0) {
    
    switch(SSL_get_error(ssl, nread)) { 
      
    case SSL_ERROR_NONE:                   /* no data */ 
      return nread;
      
    case SSL_ERROR_WANT_READ:              /* renegotiate connection */ 
    case SSL_ERROR_WANT_WRITE: 
      goto retry;
      
    case SSL_ERROR_ZERO_RETURN:            /* connection closed */ 
      fprintf(stderr, "SSL_read_wrapper: SSL connection closed!\n"); 
      return nread; 
	
    default:   
      fprintf(stderr, "SSL_read_wrapper: SSL_read failed\n"); 
      return nread;
    }
  }

  fprintf(stderr, "SSL_readn: Read %d bytes.\n", nread);

  return nread;
}


/*
 * Write n bytes to an SSL descriptor.
 */

ssize_t                        
SSL_writen(SSL *ssl, const void *vptr, size_t n)
{
  int nleft;
  ssize_t nwritten;
  const char *ptr;

  ptr = vptr;
  nleft = n;

  while(nleft > 0) {
    
    if ( (nwritten = SSL_write(ssl, ptr, nleft)) != nleft) {
      if (nwritten <= 0) {
	int error = SSL_get_error(ssl, n);
	switch(error) {

	case SSL_ERROR_WANT_READ:            /* renegotiate connection */
	case SSL_ERROR_WANT_WRITE:
	  continue;

	case SSL_ERROR_ZERO_RETURN:            /* connection closed */
	  return -1;	

	default:  
	  fprintf(stderr, "SSL_writen failed");
	}
      }
    }
    
    nleft -= nwritten;
    ptr += nwritten;
  }

  return n;
}


SSL_CTX *setup_ctx(const char *certfile, const char *keyfile)
{
  SSL_CTX *ctx;

  if((ctx = SSL_CTX_new(TLSv1_method())) == NULL) {
    fprintf(stderr, "Error creating new security context object");
    return NULL;
  }
  if(SSL_CTX_use_certificate_chain_file(ctx, certfile) != 1) {
    fprintf(stderr, "Error loading certificate from file");
    return NULL;
  }
  if(SSL_CTX_use_RSAPrivateKey_file(ctx, keyfile, SSL_FILETYPE_PEM) != 1) {
    fprintf(stderr, "Error loading private key from file");
    return NULL;
  }
  
  SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
  
  return ctx;
}
