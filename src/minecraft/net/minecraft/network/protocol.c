#include "protocol.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define MC_QUEUE_LIMIT (4u * MC_MAX_PACKET)
#define MC_STRING_LIMIT 32767u

void mc_buf_init(mc_buf *b) { memset(b,0,sizeof(*b)); }
void mc_buf_free(mc_buf *b) { free(b->data); mc_buf_init(b); }
void mc_buf_clear(mc_buf *b) { b->len=0; b->pos=0; b->failed=false; }

static bool reserve(mc_buf *b,size_t extra,size_t limit) {
    if (b->failed) return false;
    if (extra > limit || b->len > limit-extra) { b->failed=true; return false; }
    size_t need=b->len+extra;
    if (need <= b->cap) return true;
    size_t cap=b->cap ? b->cap : 256;
    while (cap < need) { if (cap > limit/2) { cap=limit; break; } cap*=2; }
    uint8_t *next=realloc(b->data,cap);
    if (!next) { b->failed=true; return false; }
    b->data=next; b->cap=cap; return true;
}

void mc_put_bytes(mc_buf *b,const void *data,size_t size) {
    if (size && !data) { b->failed=true; return; }
    size_t offset=0; bool inside=false;
    if (size && b->data && (uintptr_t)data >= (uintptr_t)b->data &&
        (uintptr_t)data-(uintptr_t)b->data < b->len) {
        offset=(size_t)((uintptr_t)data-(uintptr_t)b->data); inside=true;
        if (size > b->len-offset) { b->failed=true; return; }
    }
    if (!reserve(b,size,MC_MAX_PACKET)) return;
    if (size) memmove(b->data+b->len,inside ? b->data+offset : data,size);
    b->len+=size;
}
void mc_put_u8(mc_buf *b,uint8_t v) { mc_put_bytes(b,&v,1); }
static void put_integer(mc_buf *b,uint64_t v,unsigned bytes) {
    uint8_t wire[8]; for (unsigned i=0;i<bytes;i++) wire[bytes-1-i]=(uint8_t)(v>>(i*8));
    mc_put_bytes(b,wire,bytes);
}
void mc_put_i16(mc_buf *b,int16_t v) { put_integer(b,(uint16_t)v,2); }
void mc_put_i32(mc_buf *b,int32_t v) { put_integer(b,(uint32_t)v,4); }
void mc_put_i64(mc_buf *b,int64_t v) { put_integer(b,(uint64_t)v,8); }
void mc_put_f32(mc_buf *b,float v) { uint32_t bits; memcpy(&bits,&v,4); put_integer(b,bits,4); }
void mc_put_f64(mc_buf *b,double v) { uint64_t bits; memcpy(&bits,&v,8); put_integer(b,bits,8); }
void mc_put_varint(mc_buf *b,int32_t v) {
    uint32_t bits=(uint32_t)v;
    do { uint8_t byte=(uint8_t)(bits&127); bits>>=7; mc_put_u8(b,(uint8_t)(byte|(bits ? 128 : 0))); } while (bits);
}

bool mc_get_bytes(mc_buf *b,void *out,size_t size) {
    if (b->failed || b->pos > b->len || size > b->len-b->pos || (size && !out)) { b->failed=true; return false; }
    if (size) memcpy(out,b->data+b->pos,size);
    b->pos+=size; return true;
}
uint8_t mc_get_u8(mc_buf *b) { uint8_t v=0; mc_get_bytes(b,&v,1); return v; }
static uint64_t get_integer(mc_buf *b,unsigned bytes) {
    uint8_t wire[8]; if (!mc_get_bytes(b,wire,bytes)) return 0;
    uint64_t v=0; for (unsigned i=0;i<bytes;i++) v=(v<<8)|wire[i]; return v;
}
int16_t mc_get_i16(mc_buf *b) { uint16_t bits=(uint16_t)get_integer(b,2); int16_t v; memcpy(&v,&bits,2); return v; }
int32_t mc_get_i32(mc_buf *b) { uint32_t bits=(uint32_t)get_integer(b,4); int32_t v; memcpy(&v,&bits,4); return v; }
int64_t mc_get_i64(mc_buf *b) { uint64_t bits=get_integer(b,8); int64_t v; memcpy(&v,&bits,8); return v; }
float mc_get_f32(mc_buf *b) { uint32_t bits=(uint32_t)get_integer(b,4); float v; memcpy(&v,&bits,4); return v; }
double mc_get_f64(mc_buf *b) { uint64_t bits=get_integer(b,8); double v; memcpy(&v,&bits,8); return v; }
int32_t mc_get_varint(mc_buf *b) {
    uint32_t bits=0;
    for (unsigned i=0;i<5;i++) {
        uint8_t byte=mc_get_u8(b); if (b->failed) return 0;
        if (i==4 && (byte&0xf0)) { b->failed=true; return 0; }
        bits|=(uint32_t)(byte&127)<<(i*7);
        if (!(byte&128)) { int32_t v; memcpy(&v,&bits,4); return v; }
    }
    b->failed=true; return 0;
}

/* Return the width of one well-formed UTF-8 scalar. */
static size_t utf8_width(const uint8_t *s,size_t size,uint32_t *scalar) {
    if (!size) return 0;
    uint32_t v=s[0]; size_t n;
    if (v<128) n=1;
    else if (v>=0xc2 && v<=0xdf) { n=2; v&=31; }
    else if (v>=0xe0 && v<=0xef) { n=3; v&=15; }
    else if (v>=0xf0 && v<=0xf4) { n=4; v&=7; }
    else return 0;
    if (n>size) return 0;
    for (size_t i=1;i<n;i++) { if ((s[i]&0xc0)!=0x80) return 0; v=(v<<6)|(s[i]&63); }
    if ((n==2 && v<128) || (n==3 && v<2048) || (n==4 && v<65536) ||
        (v>=0xd800 && v<=0xdfff) || v>0x10ffff) return 0;
    if (scalar) *scalar=v;
    return n;
}
static bool valid_string(const uint8_t *s,size_t size) {
    size_t units=0;
    for (size_t i=0;i<size;) {
        uint32_t scalar; size_t n=utf8_width(s+i,size-i,&scalar);
        if (!n || !scalar) return false;
        units+=scalar>0xffff ? 2u : 1u; if (units>MC_STRING_LIMIT) return false; i+=n;
    }
    return true;
}
void mc_put_string(mc_buf *b,const char *v) {
    if (!v) { b->failed=true; return; }
    size_t size=strlen(v);
    if (size>MC_STRING_LIMIT*4u || !valid_string((const uint8_t*)v,size)) { b->failed=true; return; }
    mc_put_varint(b,(int32_t)size); mc_put_bytes(b,v,size);
}
bool mc_get_string(mc_buf *b,char *out,size_t capacity) {
    if (capacity && out) out[0]=0;
    int32_t size=mc_get_varint(b);
    if (b->failed || size<0 || (uint32_t)size>MC_STRING_LIMIT*4u || !out ||
        (size_t)size>=capacity || b->pos>b->len || (size_t)size>b->len-b->pos ||
        !valid_string(b->data+b->pos,(size_t)size)) { b->failed=true; return false; }
    if (!mc_get_bytes(b,out,(size_t)size)) return false;
    out[size]=0; return true;
}
void mc_put_position(mc_buf *b,int x,int y,int z) {
    uint64_t packed=((uint64_t)(uint32_t)x&UINT64_C(0x3ffffff))<<38 |
                    ((uint64_t)(uint32_t)y&UINT64_C(0xfff))<<26 |
                    ((uint64_t)(uint32_t)z&UINT64_C(0x3ffffff));
    put_integer(b,packed,8);
}
static int signed_coordinate(uint32_t v,unsigned bits) {
    return (int)((int64_t)v-((v&(UINT32_C(1)<<(bits-1))) ? INT64_C(1)<<bits : 0));
}
void mc_get_position(mc_buf *b,int *x,int *y,int *z) {
    uint64_t v=get_integer(b,8);
    if (x) *x=signed_coordinate((uint32_t)(v>>38),26);
    if (y) *y=signed_coordinate((uint32_t)((v>>26)&4095),12);
    if (z) *z=signed_coordinate((uint32_t)(v&0x3ffffff),26);
}

static bool conn_fail(mc_conn *c,const char *message) {
    if (!c->error[0]) snprintf(c->error,sizeof(c->error),"%s",message);
    mc_conn_close(c); return false;
}
static void queue_compact(mc_buf *b) {
    if (!b->pos) return;
    if (b->pos < b->len) memmove(b->data,b->data+b->pos,b->len-b->pos);
    b->len-=b->pos; b->pos=0;
}
bool mc_conn_send(mc_conn *c,const mc_buf *packet) {
    if (c->closed) return false;
    if (!packet || packet->failed || !packet->len || packet->len>MC_MAX_PACKET) return conn_fail(c,"Invalid outgoing packet");
    mc_buf body,frame; mc_buf_init(&body); mc_buf_init(&frame);
    if (c->compression_threshold>=0) {
        if (packet->len>=(size_t)c->compression_threshold) {
            uLongf capacity=compressBound((uLong)packet->len); uint8_t *compressed=malloc((size_t)capacity);
            if (!compressed) { conn_fail(c,"Compression allocation failed"); goto done; }
            int result=compress2(compressed,&capacity,packet->data,(uLong)packet->len,Z_DEFAULT_COMPRESSION);
            if (result==Z_OK) { mc_put_varint(&body,(int32_t)packet->len); mc_put_bytes(&body,compressed,(size_t)capacity); }
            free(compressed);
            if (result!=Z_OK) { conn_fail(c,"Packet compression failed"); goto done; }
        } else { mc_put_varint(&body,0); mc_put_bytes(&body,packet->data,packet->len); }
    } else mc_put_bytes(&body,packet->data,packet->len);
    /* The outer length is a VarInt limited to 21 bits by Java 1.8. */
    if (body.failed || body.len>=MC_MAX_PACKET) { conn_fail(c,"Outgoing frame exceeds protocol limit"); goto done; }
    mc_put_varint(&frame,(int32_t)body.len);
    if (!reserve(&frame,body.len,MC_MAX_PACKET+3u)) { conn_fail(c,"Outgoing frame allocation failed"); goto done; }
    memcpy(frame.data+frame.len,body.data,body.len); frame.len+=body.len;
    queue_compact(&c->tx);
    if (!reserve(&c->tx,frame.len,MC_QUEUE_LIMIT)) { conn_fail(c,"Outgoing queue exceeds limit"); goto done; }
    memcpy(c->tx.data+c->tx.len,frame.data,frame.len); c->tx.len+=frame.len;
done:
    mc_buf_free(&body); mc_buf_free(&frame); return !c->closed;
}

int mc_conn_next(mc_conn *c,mc_buf *packet) {
    if (c->closed) return -1;
    if (!packet || packet==&c->rx || packet==&c->tx) { conn_fail(c,"Invalid packet output buffer"); return -1; }
    if (c->rx.failed || c->rx.pos>c->rx.len) { conn_fail(c,"Invalid receive buffer"); return -1; }
    size_t available=c->rx.len-c->rx.pos; if (!available) return 0;
    uint32_t frame_size=0; size_t header=0;
    for (unsigned i=0;i<3;i++) {
        if (header>=available) {
            if (c->read_eof) { conn_fail(c,"Truncated frame header at end of stream"); return -1; }
            return 0;
        }
        uint8_t byte=c->rx.data[c->rx.pos+header++]; frame_size|=(uint32_t)(byte&127)<<(i*7);
        if (!(byte&128)) break;
        if (i==2) { conn_fail(c,"Frame length VarInt exceeds three bytes"); return -1; }
    }
    if (!frame_size || frame_size>=MC_MAX_PACKET) { conn_fail(c,"Invalid frame size"); return -1; }
    if ((size_t)frame_size>available-header) {
        if (c->read_eof) { conn_fail(c,"Truncated frame payload at end of stream"); return -1; }
        return 0;
    }
    mc_buf decoded; mc_buf_init(&decoded);
    const uint8_t *body=c->rx.data+c->rx.pos+header; size_t body_size=frame_size;
    if (c->compression_threshold>=0) {
        mc_buf view={ (uint8_t*)body,body_size,body_size,0,false };
        int32_t expanded=mc_get_varint(&view);
        if (view.failed || expanded<0 || (uint32_t)expanded>MC_MAX_PACKET) { conn_fail(c,"Invalid decompressed packet size"); goto failed; }
        body+=view.pos; body_size-=view.pos;
        if (!expanded) {
            if (!body_size || body_size>=(size_t)c->compression_threshold) { conn_fail(c,"Uncompressed packet violates compression threshold"); goto failed; }
            mc_put_bytes(&decoded,body,body_size);
        } else {
            if (expanded<c->compression_threshold || !body_size) { conn_fail(c,"Compressed packet violates compression threshold"); goto failed; }
            if (!reserve(&decoded,(size_t)expanded,MC_MAX_PACKET)) { conn_fail(c,"Decompression allocation failed"); goto failed; }
            z_stream stream; memset(&stream,0,sizeof(stream));
            stream.next_in=(Bytef*)body; stream.avail_in=(uInt)body_size;
            stream.next_out=decoded.data; stream.avail_out=(uInt)expanded;
            int result=inflateInit(&stream);
            if (result!=Z_OK) { conn_fail(c,"Decompression initialization failed"); goto failed; }
            result=inflate(&stream,Z_FINISH);
            bool complete=result==Z_STREAM_END && stream.total_out==(uLong)expanded && stream.avail_in==0;
            inflateEnd(&stream);
            if (!complete) { conn_fail(c,"Corrupt compressed packet"); goto failed; }
            decoded.len=(size_t)expanded;
        }
    } else mc_put_bytes(&decoded,body,body_size);
    if (decoded.failed || !decoded.len) { conn_fail(c,"Packet allocation failed"); goto failed; }
    /* Verify the ID is present without advancing the caller's packet cursor. */
    int32_t packet_id=mc_get_varint(&decoded);
    if (decoded.failed || packet_id<0) { conn_fail(c,"Invalid packet ID VarInt"); goto failed; }
    decoded.pos=0;
    c->rx.pos+=header+frame_size;
    if (c->rx.pos==c->rx.len) mc_buf_clear(&c->rx);
    else if (c->rx.pos>MC_MAX_PACKET) queue_compact(&c->rx);
    mc_buf_free(packet); *packet=decoded; return 1;
failed:
    mc_buf_free(&decoded); return -1;
}

/* MD5 is used only for the protocol's historical offline UUID convention. */
typedef struct { uint32_t h[4]; uint64_t length; uint8_t tail[64]; size_t used; } md5_state;
static uint32_t rotate_left(uint32_t v,unsigned n) { return (v<<n)|(v>>(32-n)); }
static void md5_block(md5_state *s,const uint8_t block[64]) {
    static const uint32_t k[64]={
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    static const unsigned shift[16]={7,12,17,22,5,9,14,20,4,11,16,23,6,10,15,21};
    uint32_t words[16]; for (unsigned i=0;i<16;i++) words[i]=(uint32_t)block[i*4]|(uint32_t)block[i*4+1]<<8|
                                                          (uint32_t)block[i*4+2]<<16|(uint32_t)block[i*4+3]<<24;
    uint32_t a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3];
    for (unsigned i=0;i<64;i++) {
        uint32_t f; unsigned g;
        if (i<16) { f=(b&c)|(~b&d); g=i; }
        else if (i<32) { f=(d&b)|(~d&c); g=(5*i+1)%16; }
        else if (i<48) { f=b^c^d; g=(3*i+5)%16; }
        else { f=c^(b|~d); g=(7*i)%16; }
        uint32_t next=b+rotate_left(a+f+k[i]+words[g],shift[(i/16)*4+i%4]);
        a=d; d=c; c=b; b=next;
    }
    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d;
}
static void md5_update(md5_state *s,const uint8_t *data,size_t size) {
    s->length+=(uint64_t)size;
    while (size) {
        size_t n=64-s->used; if (n>size) n=size;
        memcpy(s->tail+s->used,data,n); s->used+=n; data+=n; size-=n;
        if (s->used==64) { md5_block(s,s->tail); s->used=0; }
    }
}
void mc_offline_uuid(const char *name,uint8_t uuid[16]) {
    md5_state state={{0x67452301,0xefcdab89,0x98badcfe,0x10325476},0,{0},0};
    const char *prefix="OfflinePlayer:"; md5_update(&state,(const uint8_t*)prefix,strlen(prefix));
    if (name) md5_update(&state,(const uint8_t*)name,strlen(name));
    uint64_t bits=state.length*8; uint8_t padding[72]={0x80};
    size_t count=state.used<56 ? 56-state.used : 120-state.used;
    for (unsigned i=0;i<8;i++) padding[count+i]=(uint8_t)(bits>>(8*i));
    md5_update(&state,padding,count+8);
    for (unsigned i=0;i<16;i++) uuid[i]=(uint8_t)(state.h[i/4]>>(8*(i%4)));
    uuid[6]=(uint8_t)((uuid[6]&15)|48); uuid[8]=(uint8_t)((uuid[8]&63)|128);
}
void mc_uuid_string(const uint8_t uuid[16],char out[37]) {
    static const char hex[]="0123456789abcdef"; size_t pos=0;
    for (unsigned i=0;i<16;i++) {
        if (i==4 || i==6 || i==8 || i==10) out[pos++]='-';
        out[pos++]=hex[uuid[i]>>4]; out[pos++]=hex[uuid[i]&15];
    }
    out[pos]=0;
}
void mc_json_escape(const char *input,char *output,size_t capacity) {
    if (!capacity || !output) return;
    output[0]=0; if (!input) return;
    size_t length=strlen(input),pos=0;
    for (size_t i=0;i<length;) {
        unsigned char ch=(unsigned char)input[i]; char escaped[7]; const char *piece=input+i; size_t n=1,consume=1;
        switch (ch) {
            case '"': piece="\\\""; n=2; break;
            case '\\': piece="\\\\"; n=2; break;
            case '\b': piece="\\b"; n=2; break;
            case '\f': piece="\\f"; n=2; break;
            case '\n': piece="\\n"; n=2; break;
            case '\r': piece="\\r"; n=2; break;
            case '\t': piece="\\t"; n=2; break;
            default:
                if (ch<32) { snprintf(escaped,sizeof(escaped),"\\u%04x",(unsigned)ch); piece=escaped; n=6; }
                else if (ch>=128) {
                    consume=utf8_width((const uint8_t*)input+i,length-i,NULL);
                    if (!consume) { piece="\\ufffd"; n=6; consume=1; } else n=consume;
                }
                break;
        }
        if (n>=capacity-pos) break;
        memcpy(output+pos,piece,n); pos+=n; i+=consume;
    }
    output[pos]=0;
}
