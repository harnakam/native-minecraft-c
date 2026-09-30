#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "protocol.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#endif

#define MC_QUEUE_LIMIT (4u * MC_MAX_PACKET)
#define MC_POLL_BUDGET (1024u * 1024u)

static int socket_error(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}
static bool would_block(int code) {
#ifdef _WIN32
    return code==WSAEWOULDBLOCK;
#else
    return code==EWOULDBLOCK || code==EAGAIN;
#endif
}
static bool interrupted(int code) {
#ifdef _WIN32
    return code==WSAEINTR;
#else
    return code==EINTR;
#endif
}
static void set_error(char *out,size_t capacity,const char *action,int code) {
    if (out && capacity) snprintf(out,capacity,"%s (socket error %d)",action,code);
}
bool mc_net_init(void) {
#ifdef _WIN32
    WSADATA data; return WSAStartup(MAKEWORD(2,2),&data)==0;
#else
    return true;
#endif
}
void mc_net_shutdown(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}
uint64_t mc_time_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec now; if (clock_gettime(CLOCK_MONOTONIC,&now)) return 0;
    return (uint64_t)now.tv_sec*1000u+(uint64_t)now.tv_nsec/1000000u;
#endif
}
void mc_sleep_ms(unsigned ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec delay={(time_t)(ms/1000u),(long)(ms%1000u)*1000000L};
    while (nanosleep(&delay,&delay)<0 && errno==EINTR) { }
#endif
}
bool mc_socket_nonblocking(mc_socket s) {
#ifdef _WIN32
    u_long mode=1; return ioctlsocket(s,FIONBIO,&mode)==0;
#else
    int flags=fcntl(s,F_GETFL,0); return flags>=0 && fcntl(s,F_SETFL,flags|O_NONBLOCK)==0;
#endif
}
void mc_socket_close(mc_socket s) {
    if (s==MC_INVALID_SOCKET) return;
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}
static void low_latency(mc_socket s) {
    int enabled=1; setsockopt(s,IPPROTO_TCP,TCP_NODELAY,(const char*)&enabled,sizeof(enabled));
#ifdef SO_NOSIGPIPE
    setsockopt(s,SOL_SOCKET,SO_NOSIGPIPE,&enabled,sizeof(enabled));
#endif
}
mc_socket mc_net_listen(const char *host,uint16_t port,char *error,size_t error_size) {
    if (error && error_size) error[0]=0;
    char service[6]; snprintf(service,sizeof(service),"%u",(unsigned)port);
    struct addrinfo hints,*addresses=NULL; memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP; hints.ai_flags=AI_PASSIVE;
    int code=getaddrinfo(host && host[0] ? host : NULL,service,&hints,&addresses);
    if (code) { set_error(error,error_size,"Cannot resolve listen address",code); return MC_INVALID_SOCKET; }
    mc_socket listener=MC_INVALID_SOCKET; int last_error=0;
    for (struct addrinfo *address=addresses;address;address=address->ai_next) {
        mc_socket candidate=socket(address->ai_family,address->ai_socktype,address->ai_protocol);
        if (candidate==MC_INVALID_SOCKET) { last_error=socket_error(); continue; }
        int enabled=1;
#ifdef _WIN32
        setsockopt(candidate,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char*)&enabled,sizeof(enabled));
#else
        setsockopt(candidate,SOL_SOCKET,SO_REUSEADDR,&enabled,sizeof(enabled));
#endif
        if (bind(candidate,address->ai_addr,(int)address->ai_addrlen)==0 && listen(candidate,32)==0 && mc_socket_nonblocking(candidate)) {
            listener=candidate; break;
        }
        last_error=socket_error(); mc_socket_close(candidate);
    }
    freeaddrinfo(addresses);
    if (listener==MC_INVALID_SOCKET) set_error(error,error_size,"Cannot bind listening socket",last_error);
    return listener;
}

static bool connect_pending(int code) {
#ifdef _WIN32
    return code==WSAEWOULDBLOCK || code==WSAEINPROGRESS || code==WSAEALREADY;
#else
    return code==EINPROGRESS || code==EALREADY;
#endif
}
static bool wait_connect(mc_socket s,uint64_t deadline,int *last_error) {
    for (;;) {
        uint64_t now=mc_time_ms(); if (now>=deadline) {
#ifdef _WIN32
            *last_error=WSAETIMEDOUT;
#else
            *last_error=ETIMEDOUT;
#endif
            return false;
        }
        uint64_t remaining=deadline-now;
        struct timeval timeout={(long)(remaining/1000u),(long)(remaining%1000u)*1000L};
#ifndef _WIN32
        if (s>=FD_SETSIZE) { *last_error=EMFILE; return false; }
#endif
        fd_set writable,errors; FD_ZERO(&writable); FD_ZERO(&errors); FD_SET(s,&writable); FD_SET(s,&errors);
#ifdef _WIN32
        int ready=select(0,NULL,&writable,&errors,&timeout);
#else
        int ready=select(s+1,NULL,&writable,&errors,&timeout);
#endif
        if (ready<0) { *last_error=socket_error(); if (interrupted(*last_error)) continue; return false; }
        if (!ready) continue;
        int code=0;
#ifdef _WIN32
        int size=sizeof(code);
#else
        socklen_t size=sizeof(code);
#endif
        if (getsockopt(s,SOL_SOCKET,SO_ERROR,(char*)&code,&size)<0) code=socket_error();
        *last_error=code; return code==0;
    }
}
mc_socket mc_net_connect(const char *host,uint16_t port,char *error,size_t error_size) {
    if (error && error_size) error[0]=0;
    char service[6]; snprintf(service,sizeof(service),"%u",(unsigned)port);
    struct addrinfo hints,*addresses=NULL; memset(&hints,0,sizeof(hints));
    hints.ai_family=AF_UNSPEC; hints.ai_socktype=SOCK_STREAM; hints.ai_protocol=IPPROTO_TCP;
    int code=getaddrinfo(host && host[0] ? host : "127.0.0.1",service,&hints,&addresses);
    if (code) { set_error(error,error_size,"Cannot resolve server address",code); return MC_INVALID_SOCKET; }
    mc_socket connected=MC_INVALID_SOCKET; int last_error=0; uint64_t deadline=mc_time_ms()+5000;
    for (struct addrinfo *address=addresses;address && mc_time_ms()<deadline;address=address->ai_next) {
        mc_socket candidate=socket(address->ai_family,address->ai_socktype,address->ai_protocol);
        if (candidate==MC_INVALID_SOCKET) { last_error=socket_error(); continue; }
        if (!mc_socket_nonblocking(candidate)) { last_error=socket_error(); mc_socket_close(candidate); continue; }
        int result=connect(candidate,address->ai_addr,(int)address->ai_addrlen);
        if (result<0) last_error=socket_error();
        if (result==0 || (connect_pending(last_error) && wait_connect(candidate,deadline,&last_error))) {
            low_latency(candidate); connected=candidate; break;
        }
        mc_socket_close(candidate);
    }
    freeaddrinfo(addresses);
    if (connected==MC_INVALID_SOCKET) set_error(error,error_size,"Cannot connect to server",last_error);
    return connected;
}
mc_socket mc_net_accept(mc_socket listener) {
    mc_socket peer=accept(listener,NULL,NULL);
    if (peer!=MC_INVALID_SOCKET) {
        if (!mc_socket_nonblocking(peer)) { mc_socket_close(peer); return MC_INVALID_SOCKET; }
        low_latency(peer);
    }
    return peer;
}
void mc_conn_init(mc_conn *c,mc_socket s) {
    memset(c,0,sizeof(*c)); c->socket=s; c->compression_threshold=-1;
    mc_buf_init(&c->rx); mc_buf_init(&c->tx);
    if (s!=MC_INVALID_SOCKET && !mc_socket_nonblocking(s)) {
        set_error(c->error,sizeof(c->error),"Cannot configure nonblocking connection",socket_error()); mc_conn_close(c);
    }
}
void mc_conn_close(mc_conn *c) {
    mc_socket_close(c->socket); c->socket=MC_INVALID_SOCKET; c->closed=true;
    mc_buf_free(&c->rx); mc_buf_free(&c->tx);
}
static bool fail_connection(mc_conn *c,const char *message,int code) {
    if (!c->error[0]) set_error(c->error,sizeof(c->error),message,code);
    mc_conn_close(c); return false;
}
static bool rx_append(mc_buf *b,const uint8_t *data,size_t size) {
    if (b->failed || b->pos>b->len) return false;
    if (b->pos) { memmove(b->data,b->data+b->pos,b->len-b->pos); b->len-=b->pos; b->pos=0; }
    if (size>MC_QUEUE_LIMIT || b->len>MC_QUEUE_LIMIT-size) return false;
    size_t need=b->len+size;
    if (need>b->cap) {
        size_t cap=b->cap ? b->cap : 16384;
        while (cap<need) { if (cap>MC_QUEUE_LIMIT/2) { cap=MC_QUEUE_LIMIT; break; } cap*=2; }
        uint8_t *next=realloc(b->data,cap); if (!next) return false; b->data=next; b->cap=cap;
    }
    memcpy(b->data+b->len,data,size); b->len+=size; return true;
}
static bool finish_read_eof(mc_conn *c) {
    if (c->rx.pos<c->rx.len || c->tx.pos<c->tx.len) return true;
    return fail_connection(c,"Peer closed connection",0);
}
bool mc_conn_poll(mc_conn *c) {
    if (c->closed) return false;
    if (c->socket==MC_INVALID_SOCKET) return fail_connection(c,"Connection closed",0);
    if (c->tx.failed || c->tx.pos>c->tx.len) return fail_connection(c,"Invalid outgoing queue",0);
    size_t sent=0;
    while (c->tx.pos<c->tx.len && sent<MC_POLL_BUDGET) {
        size_t amount=c->tx.len-c->tx.pos; if (amount>MC_POLL_BUDGET-sent) amount=MC_POLL_BUDGET-sent;
#ifdef _WIN32
        int count=send(c->socket,(const char*)c->tx.data+c->tx.pos,(int)amount,0);
#else
#ifdef MSG_NOSIGNAL
        ssize_t count=send(c->socket,c->tx.data+c->tx.pos,amount,MSG_NOSIGNAL);
#else
        ssize_t count=send(c->socket,c->tx.data+c->tx.pos,amount,0);
#endif
#endif
        if (count>0) { c->tx.pos+=(size_t)count; sent+=(size_t)count; continue; }
        if (!count) return fail_connection(c,"Socket send returned zero",0);
        int code=socket_error(); if (interrupted(code)) continue; if (would_block(code)) break;
        return fail_connection(c,"Socket send failed",code);
    }
    if (c->tx.pos==c->tx.len) mc_buf_clear(&c->tx);
    if (c->read_eof) return finish_read_eof(c);
    uint8_t incoming[16384]; size_t received=0;
    while (received<MC_POLL_BUDGET) {
#ifdef _WIN32
        int count=recv(c->socket,(char*)incoming,sizeof(incoming),0);
#else
        ssize_t count=recv(c->socket,incoming,sizeof(incoming),0);
#endif
        if (count>0) {
            if (!rx_append(&c->rx,incoming,(size_t)count)) return fail_connection(c,"Receive queue exceeds limit or allocation failed",0);
            received+=(size_t)count; continue;
        }
        if (!count) {
            /* FIN ends reads only. Preserve buffered requests and the ability to reply. */
            c->read_eof=true;
            return finish_read_eof(c);
        }
        int code=socket_error(); if (interrupted(code)) continue; if (would_block(code)) return true;
        return fail_connection(c,"Socket receive failed",code);
    }
    return true;
}
