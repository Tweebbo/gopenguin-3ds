#include "conn.h"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#ifndef __3DS__
#include <pthread.h>
#endif

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>
#endif

#ifndef NO_TLS
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include "certs.h"
#endif

struct Conn {
  int fd;
  bool tls;
  ConnCancel cancel;
#ifndef NO_TLS
  mbedtls_ssl_context ssl;
#endif
};

#ifndef NO_TLS
static struct {
  bool ready;
  mbedtls_ctr_drbg_context drbg;
  mbedtls_x509_crt roots;
  mbedtls_ssl_config conf;
} T;
#endif

#ifndef NO_TLS
#ifdef __3DS__
static LightLock tls_lock = 1;
#define TLS_LOCK() LightLock_Lock(&tls_lock)
#define TLS_UNLOCK() LightLock_Unlock(&tls_lock)
#else
#include <pthread.h>
static pthread_mutex_t tls_lock = PTHREAD_MUTEX_INITIALIZER;
#define TLS_LOCK() pthread_mutex_lock(&tls_lock)
#define TLS_UNLOCK() pthread_mutex_unlock(&tls_lock)
#endif
#endif
static bool sockets_ready;
static unsigned socket_users;
#ifdef __3DS__
static LightLock lifetime_lock=1;
#define LIFE_LOCK() LightLock_Lock(&lifetime_lock)
#define LIFE_UNLOCK() LightLock_Unlock(&lifetime_lock)
#else
static pthread_mutex_t lifetime_lock=PTHREAD_MUTEX_INITIALIZER;
#define LIFE_LOCK() pthread_mutex_lock(&lifetime_lock)
#define LIFE_UNLOCK() pthread_mutex_unlock(&lifetime_lock)
#endif
bool conn_ready(void) { return sockets_ready; }

#ifdef __3DS__
static u32 *soc_buf;
static bool sslc_ready;
#define SOC_SIZE 0x100000
#endif

bool endpoint_parse(const char *url, Endpoint *out) {
  memset(out, 0, sizeof *out);
  const char *p = strstr(url, "://");
  if (!p) return false;
  int scheme = (int)(p - url);
  if ((scheme == 3 && !strncmp(url, "wss", 3)) || (scheme == 5 && !strncmp(url, "https", 5))) out->tls = true;
  else if ((scheme == 2 && !strncmp(url, "ws", 2)) || (scheme == 4 && !strncmp(url, "http", 4))) out->tls = false;
  else return false;
  p += 3;
  for(const char *q=p;*q;q++)if((unsigned char)*q<=32||(unsigned char)*q==127||*q=='@'||*q=='#')return false;
  int n = 0;
  while (*p && *p != ':' && *p != '/' && n < (int)sizeof out->host - 1) out->host[n++] = *p++;
  out->host[n] = 0;
  out->port = out->tls ? 443 : 80;
  if (*p == ':') {char *end;long port=strtol(p+1,&end,10);if(end==p+1||(*end&&*end!='/')||port<1||port>65535)return false;out->port=(int)port;p=end;}
  if(*p&&*p!='/')return false;
  return n > 0 && out->port > 0 && out->port < 65536;
}
bool endpoint_is_lan(const Endpoint *endpoint){unsigned a,b,c,d;int used=0;if(!strcmp(endpoint->host,"localhost"))return true;if(sscanf(endpoint->host,"%u.%u.%u.%u%n",&a,&b,&c,&d,&used)!=4||endpoint->host[used]||a>255||b>255||c>255||d>255)return false;return a==127||a==10||(a==192&&b==168)||(a==172&&b>=16&&b<=31);}

#ifndef NO_TLS
/** Own entropy source: sslc (like devkitPro), then ps:ps and, as a last resort, the system clock. */
static int entropy(void *ctx, unsigned char *out, size_t len) {
  (void)ctx;
#ifdef __3DS__
  if (sslc_ready && R_SUCCEEDED(sslcGenerateRandomData(out, len))) return 0;
  if (R_SUCCEEDED(psInit())) {
    Result r = PS_GenerateRandomBytes(out, len);
    psExit();
    if (R_SUCCEEDED(r)) return 0;
  }
  u64 x = svcGetSystemTick() ^ osGetTime() * 0x9E3779B97F4A7C15ull;
  for (size_t i = 0; i < len; i++) { x ^= svcGetSystemTick(); x ^= x << 13; x ^= x >> 7; x ^= x << 17; out[i] = (u8)(x >> 24); }
  return 0;
#else
  FILE *f = fopen("/dev/urandom", "rb");
  if (!f || fread(out, 1, len, f) != len) { if (f) fclose(f); return -1; }
  fclose(f);
  return 0;
#endif
}

/* A dead clock battery must not disable hostname and certificate-chain checks. */
static int verify_ignore_clock(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags) {
  (void)data; (void)crt; (void)depth;
  *flags &= ~(uint32_t)(MBEDTLS_X509_BADCERT_EXPIRED | MBEDTLS_X509_BADCERT_FUTURE |
                        MBEDTLS_X509_BADCRL_EXPIRED | MBEDTLS_X509_BADCRL_FUTURE);
  return 0;
}

static int locked_random(void *ctx, unsigned char *out, size_t len) {
  TLS_LOCK();
  int result = mbedtls_ctr_drbg_random(ctx, out, len);
  TLS_UNLOCK();
  return result;
}

static bool tls_init(char *err, int err_cap) {
  if (T.ready) return true;
#ifdef __3DS__
  if (!sslc_ready) sslc_ready = R_SUCCEEDED(sslcInit(0));
#endif
  mbedtls_ctr_drbg_init(&T.drbg);
  mbedtls_x509_crt_init(&T.roots);
  mbedtls_ssl_config_init(&T.conf);
  const char *who = "tweebbo-3ds";
  int r = mbedtls_ctr_drbg_seed(&T.drbg, entropy, NULL, (const u8 *)who, strlen(who));
  if (r) { snprintf(err, (size_t)err_cap, "Secure random generator failed (-0x%04x).", (unsigned)-r); return false; }
  r = mbedtls_x509_crt_parse(&T.roots, (const u8 *)TRUSTED_ROOTS, strlen(TRUSTED_ROOTS) + 1);
  if (r < 0) { snprintf(err, (size_t)err_cap, "Certificates could not be loaded (-0x%04x).", (unsigned)-r); return false; }
#ifdef GP_LAN_ONLY
  FILE *test_root = fopen("romfs:/data/test-root.pem", "rb");
  if (test_root) {
    unsigned char pem[4096];
    size_t bytes = fread(pem, 1, sizeof pem - 1, test_root);
    bool valid = !ferror(test_root) && bytes && bytes < sizeof pem - 1;
    fclose(test_root);
    pem[bytes] = 0;
    if (!valid || mbedtls_x509_crt_parse(&T.roots, pem, bytes + 1)) {
      snprintf(err, (size_t)err_cap, "The local test certificate could not be loaded.");
      mbedtls_ssl_config_free(&T.conf);
      mbedtls_x509_crt_free(&T.roots);
      mbedtls_ctr_drbg_free(&T.drbg);
      return false;
    }
  }
#endif
  mbedtls_ssl_config_defaults(&T.conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
  mbedtls_ssl_conf_authmode(&T.conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  mbedtls_ssl_conf_ca_chain(&T.conf, &T.roots, NULL);
  mbedtls_ssl_conf_verify(&T.conf, verify_ignore_clock, NULL);
  mbedtls_ssl_conf_rng(&T.conf, locked_random, &T.drbg);
  T.ready = true;
  return true;
}
#endif

bool conn_global_init(char *err, int err_cap) {
  (void)err_cap;
  err[0] = 0;
  LIFE_LOCK();
  if(socket_users){socket_users++;LIFE_UNLOCK();return true;}
#ifdef __3DS__
  if (!soc_buf) {
    soc_buf = memalign(0x1000, SOC_SIZE);
    if (!soc_buf) { snprintf(err, (size_t)err_cap, "Not enough memory for the network."); LIFE_UNLOCK();return false; }
    Result r = socInit(soc_buf, SOC_SIZE);
    if (R_FAILED(r)) {
      free(soc_buf);
      soc_buf = NULL;
      snprintf(err, (size_t)err_cap, "Network service unavailable (0x%08lX). Is Wi-Fi on?", (unsigned long)r);
      LIFE_UNLOCK();return false;
    }
  }
#endif
  sockets_ready = true;
  socket_users=1;LIFE_UNLOCK();
  return true;
}

void conn_global_exit(void) {
  LIFE_LOCK();if(!socket_users||--socket_users){LIFE_UNLOCK();return;}
#ifndef NO_TLS
  if (T.ready) {
    mbedtls_ssl_config_free(&T.conf);
    mbedtls_x509_crt_free(&T.roots);
    mbedtls_ctr_drbg_free(&T.drbg);
    T.ready = false;
  }
#endif
#ifdef __3DS__
  if (sslc_ready) { sslcExit(); sslc_ready = false; }
  if (soc_buf) { socExit(); free(soc_buf); soc_buf = NULL; }
#endif
  sockets_ready = false;LIFE_UNLOCK();
}

bool conn_cancelled(const ConnCancel *cancel){return cancel&&cancel->requested&&cancel->requested(cancel->user);}
static bool wait_fd_mode(int fd, short events, int timeout_ms,const ConnCancel *cancel,bool connecting) {
  u64 begin=now_ms();
  do{
    if(conn_cancelled(cancel)){errno=ECANCELED;return false;}
    int remaining=timeout_ms-(int)(now_ms()-begin);if(remaining<0)remaining=0;
    struct pollfd p = {.fd = fd, .events = events};
    int result=poll(&p,1,remaining>50?50:remaining);
    if(result>0){
      if(connecting&&(p.revents&(POLLERR|POLLHUP|POLLNVAL)))return false;
      return (p.revents & (events | POLLERR | POLLHUP | POLLNVAL))!=0;
    }
    if(result<0&&errno!=EINTR)return false;
  }while(now_ms()-begin<(u64)(timeout_ms>0?timeout_ms:0));
  return false;
}
static bool wait_fd(int fd, short events, int timeout_ms,const ConnCancel *cancel){return wait_fd_mode(fd,events,timeout_ms,cancel,false);}

static bool would_block(void) { return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS; }

#ifndef NO_TLS
static int bio_send(void *ctx, const unsigned char *buf, size_t len) {
  int n = (int)send(((Conn *)ctx)->fd, buf, len, 0);
  if (n >= 0) return n;
  return would_block() ? MBEDTLS_ERR_SSL_WANT_WRITE : MBEDTLS_ERR_NET_SEND_FAILED;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len) {
  int n = (int)recv(((Conn *)ctx)->fd, buf, len, 0);
  if (n > 0) return n;
  if (n == 0) return MBEDTLS_ERR_NET_CONN_RESET;
  return would_block() ? MBEDTLS_ERR_SSL_WANT_READ : MBEDTLS_ERR_NET_RECV_FAILED;
}
#endif

static Conn *open_socket(const Endpoint *ep, int timeout_ms, char *err, int err_cap, bool download,const ConnCancel *cancel) {
  (void)download;
  err[0] = 0;
  if(conn_cancelled(cancel)){snprintf(err,(size_t)err_cap,"Connection cancelled.");return NULL;}
  char port[8];
  snprintf(port, sizeof port, "%d", ep->port);
  struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons((u16)ep->port)};
  unsigned octetA,octetB,octetC,octetD;int used=0;
  if(sscanf(ep->host,"%u.%u.%u.%u%n",&octetA,&octetB,&octetC,&octetD,&used)==4&&!ep->host[used]&&octetA<256&&octetB<256&&octetC<256&&octetD<256)address.sin_addr.s_addr=htonl(octetA<<24|octetB<<16|octetC<<8|octetD);
  else{
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *res = NULL;
    if (getaddrinfo(ep->host, port, &hints, &res) || !res) {snprintf(err, (size_t)err_cap, "Could not find %s.", ep->host);return NULL;}
    memcpy(&address,res->ai_addr,sizeof address);freeaddrinfo(res);
  }
  if(conn_cancelled(cancel)){snprintf(err,(size_t)err_cap,"Connection cancelled.");return NULL;}
  int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (fd < 0) {snprintf(err, (size_t)err_cap, "No network (socket error %d).", errno); return NULL; }
  if(!download){int enabled=1;setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&enabled,sizeof enabled);}
#ifdef __3DS__
  if (download) {
    // SOC shared memory does not set the TCP window negotiated at connect time.
    int bytes = 48 * 1024;
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof bytes);
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bytes, sizeof bytes);
  }
#endif
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  int r = connect(fd,(struct sockaddr *)&address,sizeof address);
  if (r < 0 && !would_block()) { close(fd); snprintf(err, (size_t)err_cap, "Connection refused (%d).", errno); return NULL; }
  if (r < 0) {
    int so = 0;
    socklen_t sl = sizeof so;
    bool ready=wait_fd_mode(fd,POLLOUT,timeout_ms,cancel,true);
    // SOC firmware can report a stale SO_ERROR; an established peer is authoritative.
    // A failed connection must not reach getpeername: Azahar translates its uninitialised output.
    struct sockaddr_in peer={0};socklen_t peerLength=sizeof peer;
    bool connected=ready&&getpeername(fd,(struct sockaddr *)&peer,&peerLength)==0;
    if(!connected)getsockopt(fd,SOL_SOCKET,SO_ERROR,&so,&sl);
    if (!connected||conn_cancelled(cancel)) {
      close(fd);
      snprintf(err, (size_t)err_cap, "Could not reach %s:%d (error %d).", ep->host, ep->port, so);
      return NULL;
    }
  }
#ifndef NO_TLS
  if (ep->tls) {
    TLS_LOCK();
    bool ok = tls_init(err, err_cap);
    TLS_UNLOCK();
    if (!ok) { close(fd); return NULL; }
  }
#endif
  Conn *c = calloc(1, sizeof *c);
  if (!c) { close(fd); snprintf(err, (size_t)err_cap, "Not enough memory for the connection."); return NULL; }
  c->fd = fd;
  c->tls = ep->tls;
  if(cancel)c->cancel=*cancel;
  if (!c->tls) return c;
#ifdef NO_TLS
  snprintf(err, (size_t)err_cap, "Secure connections are not available.");
  close(fd); free(c);
  return NULL;
#else
  mbedtls_ssl_init(&c->ssl);
  if (mbedtls_ssl_setup(&c->ssl, &T.conf) || mbedtls_ssl_set_hostname(&c->ssl, ep->host)) {
    snprintf(err, (size_t)err_cap, "Secure connection setup failed.");
    conn_close(c);
    return NULL;
  }
  mbedtls_ssl_set_bio(&c->ssl, c, bio_send, bio_recv, NULL);
  int spent = 0;
  for (;;) {
    if(conn_cancelled(cancel)){snprintf(err,(size_t)err_cap,"Connection cancelled.");conn_close(c);return NULL;}
    r = mbedtls_ssl_handshake(&c->ssl);
    if (!r) break;
    if ((r != MBEDTLS_ERR_SSL_WANT_READ && r != MBEDTLS_ERR_SSL_WANT_WRITE) || spent > timeout_ms) {
      u32 flags = mbedtls_ssl_get_verify_result(&c->ssl);
      if (flags && flags != (u32)-1) snprintf(err, (size_t)err_cap, "The server certificate is not trusted (check the console date).");
      else snprintf(err, (size_t)err_cap, "Secure connection failed (-0x%04x).", (unsigned)-r);
      conn_close(c);
      return NULL;
    }
    wait_fd(fd, r == MBEDTLS_ERR_SSL_WANT_READ ? POLLIN : POLLOUT, 100,cancel);
    spent += 100;
  }
  u32 vf = mbedtls_ssl_get_verify_result(&c->ssl);
  if (vf && vf != (u32)-1) dbg_log("tls: certificate not fully verified (0x%08lx); continuing", (unsigned long)vf);
  return c;
#endif
}

Conn *conn_open(const Endpoint *ep, int timeout_ms, char *err, int err_cap) {
  return open_socket(ep, timeout_ms, err, err_cap, false,NULL);
}

Conn *conn_open_download(const Endpoint *ep, int timeout_ms, char *err, int err_cap) {
  return open_socket(ep, timeout_ms, err, err_cap, true,NULL);
}
Conn *conn_open_cancel(const Endpoint *ep,int timeout_ms,char *err,int err_cap,bool download,const ConnCancel *cancel){return open_socket(ep,timeout_ms,err,err_cap,download,cancel);}

void conn_close(Conn *c) {
  if (!c) return;
#ifndef NO_TLS
  if (c->tls) {
    mbedtls_ssl_close_notify(&c->ssl);
    mbedtls_ssl_free(&c->ssl);
  }
#endif
  close(c->fd);
  free(c);
}

bool conn_write(Conn *c, const void *data, int len, int timeout_ms) {
  const u8 *p = data;
  int spent = 0;
  while (len > 0) {
    if(conn_cancelled(&c->cancel))return false;
    int n;
#ifndef NO_TLS
    if (c->tls) {
      n = mbedtls_ssl_write(&c->ssl, p, (size_t)len);
      if (n == MBEDTLS_ERR_SSL_WANT_WRITE || n == MBEDTLS_ERR_SSL_WANT_READ) n = -2;
      else if (n < 0) return false;
    } else
#endif
    {
      n = (int)send(c->fd, p, (size_t)len, 0);
      if (n < 0) { if (!would_block()) return false; n = -2; }
    }
    if (n == -2) {
      if (spent > timeout_ms) return false;
      wait_fd(c->fd, POLLOUT, 50,&c->cancel);
      spent += 50;
      continue;
    }
    p += n;
    len -= n;
  }
  return true;
}

int conn_read(Conn *c, void *buf, int cap, int timeout_ms) {
  if(conn_cancelled(&c->cancel))return -1;
#ifndef NO_TLS
  if (c->tls && mbedtls_ssl_get_bytes_avail(&c->ssl) == 0 && !wait_fd(c->fd, POLLIN, timeout_ms,&c->cancel)) return conn_cancelled(&c->cancel)?-1:0;
  if (c->tls) {
    int n = mbedtls_ssl_read(&c->ssl, buf, (size_t)cap);
    if (n > 0) return n;
    if (n == MBEDTLS_ERR_SSL_WANT_READ || n == MBEDTLS_ERR_SSL_WANT_WRITE) return 0;
    return -1;
  }
#endif
  if (!wait_fd(c->fd, POLLIN, timeout_ms,&c->cancel)) return conn_cancelled(&c->cancel)?-1:0;
  int n = (int)recv(c->fd, buf, (size_t)cap, 0);
  if (n > 0) return n;
  if (n < 0 && would_block()) return 0;
  return -1;
}
