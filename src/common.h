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

#ifndef _COMMON_H_
#define _COMMON_H_

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include "avahi.h"

typedef struct {
  volatile kcm_avahi_connect_info_t *host;
  volatile unsigned int port;
} client_params_t;

ssize_t readn(int fd, void *vptr, size_t n); 
ssize_t writen(int fd, const void *vptr, size_t n); 
void tunnel(int localsock, int remotesock, SSL *remotessl);

unsigned short choose_random_port(void);
int listen_on_any_tcp_port(void);
int listen_on_any_tcp_port_locally(void);
int make_tcpip_connection(char *hostname, unsigned short port);
int make_tcpip_connection_locally(unsigned short port);


int init_OpenSSL(void);


/* SSL threading functions */

int THREAD_setup(void);
int THREAD_cleanup(void);


/* SSL static locking */

#define MUTEX_TYPE       pthread_mutex_t
#define MUTEX_SETUP(x)   pthread_mutex_init(&(x), NULL);  /* NULL indicates
							  * default mutex
							  * attributes */
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define THREAD_ID        pthread_self()


/* SSL mutex state */

static MUTEX_TYPE *mutex_buf;

static void locking_function(int mode, int n, const char * file, int line);
static unsigned long id_function(void);


/* SSL dynamic locking */

struct CRYPTO_dynlock_value {
  MUTEX_TYPE mutex;
};

static struct CRYPTO_dynlock_value *dyn_create_function(const char *file,
							int line);

static void dyn_lock_function(int mode, struct CRYPTO_dynlock_value *l,
			      const char *file, int line);

static void dyn_destroy_function(struct CRYPTO_dynlock_value *l,
				 const char *file, int line);


/* SSL seeding */

int seed_prng(int bytes);
SSL_CTX *setup_ctx(const char *certfile, const char *keyfile);
ssize_t SSL_read_wrapper(SSL *ssl, void *vptr, size_t n);
ssize_t SSL_writen(SSL *, const void *, size_t);

extern SSL_CTX *ctx;

#endif /* _COMMON_H_ */
