#ifndef C919_PROTOCOL_H
#define C919_PROTOCOL_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET mc_socket;
#define MC_INVALID_SOCKET INVALID_SOCKET
#else
typedef int mc_socket;
#define MC_INVALID_SOCKET (-1)
#endif
#define MC_PROTOCOL_VERSION 47
#define MC_MAX_PACKET (2u * 1024u * 1024u)
typedef struct { uint8_t *data; size_t len, cap, pos; bool failed; } mc_buf;
void mc_buf_init(mc_buf *b);
void mc_buf_free(mc_buf *b);
void mc_buf_clear(mc_buf *b);
void mc_put_bytes(mc_buf *b, const void *data, size_t size);
void mc_put_u8(mc_buf *b, uint8_t v);
void mc_put_i16(mc_buf *b, int16_t v);
void mc_put_i32(mc_buf *b, int32_t v);
void mc_put_i64(mc_buf *b, int64_t v);
void mc_put_f32(mc_buf *b, float v);
void mc_put_f64(mc_buf *b, double v);
void mc_put_varint(mc_buf *b, int32_t v);
void mc_put_string(mc_buf *b, const char *v);
void mc_put_position(mc_buf *b, int x, int y, int z);
uint8_t mc_get_u8(mc_buf *b);
int16_t mc_get_i16(mc_buf *b);
int32_t mc_get_i32(mc_buf *b);
int64_t mc_get_i64(mc_buf *b);
float mc_get_f32(mc_buf *b);
double mc_get_f64(mc_buf *b);
int32_t mc_get_varint(mc_buf *b);
bool mc_get_string(mc_buf *b, char *out, size_t capacity);
bool mc_get_bytes(mc_buf *b, void *out, size_t size);
void mc_get_position(mc_buf *b, int *x, int *y, int *z);
typedef struct {
    mc_socket socket;
    mc_buf rx, tx;
    int compression_threshold;
    bool read_eof; /* Peer has ended writes; buffered packets and replies may remain. */
    bool closed;
    char error[160];
} mc_conn;
bool mc_net_init(void);
void mc_net_shutdown(void);
uint64_t mc_time_ms(void);
void mc_sleep_ms(unsigned ms);
bool mc_socket_nonblocking(mc_socket s);
void mc_socket_close(mc_socket s);
mc_socket mc_net_listen(const char *host, uint16_t port, char *error, size_t error_size);
mc_socket mc_net_connect(const char *host, uint16_t port, char *error, size_t error_size);
mc_socket mc_net_accept(mc_socket listener);
void mc_conn_init(mc_conn *c, mc_socket s);
void mc_conn_close(mc_conn *c);
bool mc_conn_poll(mc_conn *c);
bool mc_conn_send(mc_conn *c, const mc_buf *packet);
/* 1: complete owning packet, 0: incomplete, -1: protocol/transport failure. */
int mc_conn_next(mc_conn *c, mc_buf *packet);
/* Native UUID.nameUUIDFromBytes dependency: all bytes, including NUL, enter
   MD5 before the source version-3/variant bits are set. No packet/name limit. */
bool mc_name_uuid_from_bytes(const uint8_t *data,size_t length,uint8_t uuid[16]);
void mc_offline_uuid(const char *name, uint8_t uuid[16]);
void mc_uuid_string(const uint8_t uuid[16], char out[37]);
void mc_json_escape(const char *input, char *output, size_t capacity);
#endif
