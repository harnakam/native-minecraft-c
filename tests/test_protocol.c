#include "network/protocol.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

static unsigned checks;
#define CHECK(v) do { ++checks; if (!(v)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #v); exit(1); } } while (0)

static void test_wire_values(void) {
    mc_buf b; mc_buf_init(&b);
    mc_put_u8(&b, 0xa5); mc_put_i16(&b, -2); mc_put_i32(&b, 0x12345678);
    mc_put_i64(&b, INT64_C(0x0102030405060708)); mc_put_f32(&b, 1.0f); mc_put_f64(&b, -2.0);
    const uint8_t expected[] = {0xa5,0xff,0xfe,0x12,0x34,0x56,0x78,1,2,3,4,5,6,7,8,
                               0x3f,0x80,0,0,0xc0,0,0,0,0,0,0,0};
    CHECK(!b.failed && b.len == sizeof(expected)); CHECK(memcmp(b.data, expected, sizeof(expected)) == 0);
    CHECK(mc_get_u8(&b) == 0xa5); CHECK(mc_get_i16(&b) == -2); CHECK(mc_get_i32(&b) == 0x12345678);
    CHECK(mc_get_i64(&b) == INT64_C(0x0102030405060708)); CHECK(mc_get_f32(&b) == 1.0f); CHECK(mc_get_f64(&b) == -2.0);
    CHECK(mc_get_u8(&b) == 0 && b.failed); mc_buf_clear(&b); CHECK(!b.failed && b.pos == 0 && b.len == 0);
    mc_buf_free(&b); mc_buf_free(&b);
}

static void test_varints(void) {
    const struct { int32_t value; uint8_t bytes[5]; size_t n; } cases[] = {
        {0,{0},1}, {127,{0x7f},1}, {128,{0x80,1},2}, {300,{0xac,2},2},
        {INT32_MAX,{0xff,0xff,0xff,0xff,7},5}, {-1,{0xff,0xff,0xff,0xff,0x0f},5},
        {INT32_MIN,{0x80,0x80,0x80,0x80,8},5}
    };
    mc_buf b; mc_buf_init(&b);
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        mc_buf_clear(&b); mc_put_varint(&b,cases[i].value);
        CHECK(b.len == cases[i].n && memcmp(b.data,cases[i].bytes,b.len) == 0);
        CHECK(mc_get_varint(&b) == cases[i].value && !b.failed);
    }
    const uint8_t too_long[] = {0x80,0x80,0x80,0x80,0x80,0};
    mc_buf_clear(&b); mc_put_bytes(&b,too_long,sizeof(too_long)); mc_get_varint(&b); CHECK(b.failed);
    const uint8_t overflow[] = {0xff,0xff,0xff,0xff,0x10};
    mc_buf_clear(&b); mc_put_bytes(&b,overflow,sizeof(overflow)); mc_get_varint(&b); CHECK(b.failed);
    mc_buf_clear(&b); mc_put_u8(&b,0x80); mc_get_varint(&b); CHECK(b.failed);
    mc_buf_free(&b);
}

static void test_strings_and_bounds(void) {
    mc_buf b; mc_buf_init(&b); char out[16];
    mc_put_string(&b,"Tokyo\xe6\x9d\xb1\xe4\xba\xac"); CHECK(b.data[0] == 11);
    CHECK(mc_get_string(&b,out,sizeof(out)) && strcmp(out,"Tokyo\xe6\x9d\xb1\xe4\xba\xac") == 0);
    mc_buf_clear(&b); mc_put_string(&b,"long"); CHECK(!mc_get_string(&b,out,3) && b.failed && out[0] == 0);
    mc_buf_clear(&b); mc_put_varint(&b,-1); CHECK(!mc_get_string(&b,out,sizeof(out)) && b.failed);
    mc_buf_clear(&b); mc_put_u8(&b,3); mc_put_u8(&b,'x'); CHECK(!mc_get_string(&b,out,sizeof(out)) && b.failed);
    mc_buf_clear(&b); const uint8_t invalid[] = {2,0xc0,0x80}; mc_put_bytes(&b,invalid,sizeof(invalid));
    CHECK(!mc_get_string(&b,out,sizeof(out)) && b.failed);
    mc_buf_clear(&b); mc_put_bytes(&b,"x",(size_t)MC_MAX_PACKET+1); CHECK(b.failed && b.len == 0);
    mc_buf_clear(&b); CHECK(!mc_get_bytes(&b,out,1) && b.failed);
    mc_buf_free(&b);
}

static void test_position(void) {
    mc_buf b; mc_buf_init(&b); int x,y,z;
    mc_put_position(&b,1,2,3);
    const uint8_t expected[] = {0,0,0,0x40,0x08,0,0,3};
    CHECK(b.len == 8 && memcmp(b.data,expected,8) == 0);
    mc_get_position(&b,&x,&y,&z); CHECK(x == 1 && y == 2 && z == 3);
    mc_buf_clear(&b); mc_put_position(&b,-33554432,-2048,33554431); mc_get_position(&b,&x,&y,&z);
    CHECK(x == -33554432 && y == -2048 && z == 33554431);
    mc_buf_clear(&b); mc_put_position(&b,-1,255,-1); mc_get_position(&b,&x,&y,&z); CHECK(x == -1 && y == 255 && z == -1);
    mc_buf_free(&b);
}

static void test_uuid_and_json(void) {
    uint8_t id[16]; char text[37];
    mc_offline_uuid("Steve",id); mc_uuid_string(id,text); CHECK(strcmp(text,"5627dd98-e6be-3c21-b8a8-e92344183641") == 0);
    mc_offline_uuid("Notch",id); mc_uuid_string(id,text); CHECK(strcmp(text,"b50ad385-829d-3141-a216-7e7d7539ba7f") == 0);
    char long_name[81]; memset(long_name,'a',80); long_name[80]=0; mc_offline_uuid(long_name,id); mc_uuid_string(id,text);
    CHECK(strcmp(text,"1ca08cb7-08c8-3e18-8cac-37d9213bf972") == 0);
    char out[80]; mc_json_escape("line\n\t\"\\\001",out,sizeof(out)); CHECK(strcmp(out,"line\\n\\t\\\"\\\\\\u0001") == 0);
    mc_json_escape("\nX",out,2); CHECK(out[0] == 0); mc_json_escape("ok",out,1); CHECK(out[0] == 0);
    mc_json_escape("utf8\xe6\x9d\xb1",out,sizeof(out)); CHECK(strcmp(out,"utf8\xe6\x9d\xb1") == 0);
    mc_json_escape("\xe6\x9d\xb1",out,3); CHECK(out[0] == 0);
}

static void transfer_frame(mc_conn *from,mc_conn *to) {
    mc_put_bytes(&to->rx,from->tx.data+from->tx.pos,from->tx.len-from->tx.pos); mc_buf_clear(&from->tx);
}

static void test_frames(void) {
    mc_conn writer,reader; mc_conn_init(&writer,MC_INVALID_SOCKET); mc_conn_init(&reader,MC_INVALID_SOCKET);
    mc_buf p,got; mc_buf_init(&p); mc_buf_init(&got); mc_put_varint(&p,0x02); mc_put_string(&p,"hello");
    CHECK(mc_conn_send(&writer,&p)); const uint8_t wire[]={7,2,5,'h','e','l','l','o'};
    CHECK(writer.tx.len == sizeof(wire) && memcmp(writer.tx.data,wire,sizeof(wire)) == 0);
    mc_put_bytes(&reader.rx,wire,2); CHECK(mc_conn_next(&reader,&got) == 0 && !reader.closed);
    mc_put_bytes(&reader.rx,wire+2,sizeof(wire)-2); CHECK(mc_conn_next(&reader,&got) == 1);
    CHECK(got.len == p.len && memcmp(got.data,p.data,p.len) == 0); CHECK(mc_conn_next(&reader,&got) == 0);
    mc_buf_clear(&writer.tx); writer.compression_threshold=32; reader.compression_threshold=32;
    CHECK(mc_conn_send(&writer,&p)); CHECK(writer.tx.data[0] == 8 && writer.tx.data[1] == 0); transfer_frame(&writer,&reader);
    CHECK(mc_conn_next(&reader,&got) == 1 && got.len == p.len);
    mc_buf_clear(&p); mc_put_varint(&p,0x21); for (int i=0;i<4096;i++) mc_put_u8(&p,(uint8_t)(i%17));
    CHECK(mc_conn_send(&writer,&p)); transfer_frame(&writer,&reader); CHECK(mc_conn_next(&reader,&got) == 1);
    CHECK(got.len == p.len && memcmp(got.data,p.data,p.len) == 0);
    /* An independent zlib encoder supplies a compressed frame, avoiding a symmetric codec-only test. */
    const uint8_t body[]={0,42,42,42,42}; uint8_t zipped[80]; uLongf zipped_size=sizeof(zipped);
    CHECK(compress2(zipped,&zipped_size,body,sizeof(body),Z_BEST_SPEED) == Z_OK);
    mc_buf_clear(&writer.tx); mc_put_varint(&writer.tx,(int32_t)(zipped_size+1)); mc_put_varint(&writer.tx,sizeof(body));
    mc_put_bytes(&writer.tx,zipped,(size_t)zipped_size); reader.compression_threshold=1; transfer_frame(&writer,&reader);
    CHECK(mc_conn_next(&reader,&got) == 1 && got.len == sizeof(body) && memcmp(got.data,body,sizeof(body)) == 0);
    mc_buf_free(&p); mc_buf_free(&got); mc_conn_close(&writer); mc_conn_close(&reader); mc_conn_close(&reader);
}

static void test_invalid_frames(void) {
    const struct { uint8_t data[10]; size_t n; int threshold; } cases[]={
        {{0},1,-1}, {{0x80,0x80,0x80,0},4,-1}, {{0xff,0xff,0x7f},3,-1},
        {{1,0},2,1}, {{2,1,0},3,2}, {{3,5,0x78,0},4,1},
        {{5,0x81,0x80,0x80,1,0},6,1}, {{5,0xff,0xff,0xff,0xff,0x0f},6,-1}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        mc_conn c; mc_conn_init(&c,MC_INVALID_SOCKET); c.compression_threshold=cases[i].threshold;
        mc_buf out; mc_buf_init(&out); mc_put_bytes(&c.rx,cases[i].data,cases[i].n);
        /* A maximum legal length with no payload is incomplete, not corrupt. */
        int result=mc_conn_next(&c,&out);
        if (i == 2) CHECK(result == 0 && !c.closed);
        else CHECK(result == -1 && c.closed && c.error[0]);
        mc_conn_close(&c); mc_buf_free(&out);
    }
    mc_conn c; mc_conn_init(&c,MC_INVALID_SOCKET); mc_buf p; mc_buf_init(&p); p.failed=true;
    CHECK(!mc_conn_send(&c,&p) && c.closed && c.error[0]); mc_conn_close(&c); mc_buf_free(&p);
}

static void test_frame_and_queue_limits(void) {
    mc_conn writer,reader; mc_conn_init(&writer,MC_INVALID_SOCKET); mc_conn_init(&reader,MC_INVALID_SOCKET);
    mc_buf p,out; mc_buf_init(&p); mc_buf_init(&out);
    uint8_t *body=calloc(1,MC_MAX_PACKET); CHECK(body != NULL);
    mc_put_bytes(&p,body,MC_MAX_PACKET-1); CHECK(mc_conn_send(&writer,&p));
    CHECK(writer.tx.len == (size_t)MC_MAX_PACKET+2);
    reader.rx.data=malloc(writer.tx.len); CHECK(reader.rx.data != NULL);
    reader.rx.cap=reader.rx.len=writer.tx.len; memcpy(reader.rx.data,writer.tx.data,writer.tx.len);
    CHECK(mc_conn_next(&reader,&out) == 1 && out.len == MC_MAX_PACKET-1 && out.data != p.data);
    CHECK(mc_conn_send(&writer,&p)); CHECK(mc_conn_send(&writer,&p));
    CHECK(!mc_conn_send(&writer,&p) && writer.closed && writer.error[0]);
    mc_conn_close(&writer); mc_conn_init(&writer,MC_INVALID_SOCKET); writer.compression_threshold=1; reader.compression_threshold=1;
    mc_buf_clear(&p); mc_put_bytes(&p,body,MC_MAX_PACKET); CHECK(mc_conn_send(&writer,&p)); transfer_frame(&writer,&reader);
    CHECK(mc_conn_next(&reader,&out) == 1 && out.len == MC_MAX_PACKET);
    free(body); mc_buf_free(&p); mc_buf_free(&out); mc_conn_close(&writer); mc_conn_close(&reader);
}

static void test_loopback_transport(void) {
    CHECK(mc_net_init()); char error[160]; mc_socket listener=mc_net_listen("127.0.0.1",0,error,sizeof(error));
    CHECK(listener != MC_INVALID_SOCKET); struct sockaddr_in addr;
#ifdef _WIN32
    int addr_size=sizeof(addr);
#else
    socklen_t addr_size=sizeof(addr);
#endif
    CHECK(getsockname(listener,(struct sockaddr*)&addr,&addr_size) == 0);
    mc_socket client=mc_net_connect("127.0.0.1",ntohs(addr.sin_port),error,sizeof(error)); CHECK(client != MC_INVALID_SOCKET);
    mc_socket peer=MC_INVALID_SOCKET; for (int i=0;i<100 && peer == MC_INVALID_SOCKET;i++) { peer=mc_net_accept(listener); if (peer == MC_INVALID_SOCKET) mc_sleep_ms(1); }
    CHECK(peer != MC_INVALID_SOCKET); mc_conn sender,receiver; mc_conn_init(&sender,client); mc_conn_init(&receiver,peer);
    mc_buf p,out; mc_buf_init(&p); mc_buf_init(&out); mc_put_varint(&p,0); mc_put_varint(&p,-123);
    CHECK(mc_conn_send(&sender,&p)); int result=0;
    for (int i=0;i<100 && result == 0;i++) { CHECK(mc_conn_poll(&sender)); CHECK(mc_conn_poll(&receiver)); result=mc_conn_next(&receiver,&out); if (!result) mc_sleep_ms(1); }
    CHECK(result == 1 && mc_get_varint(&out) == 0 && mc_get_varint(&out) == -123 && !out.failed);
    CHECK(sender.tx.len == 0);
    /* More than one poll budget forces a queued partial send across real TCP. */
    uint8_t *large=malloc(300000); CHECK(large != NULL);
    for (size_t i=0;i<300000;i++) large[i]=(uint8_t)(i*37u);
    mc_buf_clear(&p); mc_put_varint(&p,1); mc_put_bytes(&p,large,300000);
    for (int i=0;i<16;i++) CHECK(mc_conn_send(&sender,&p));
    CHECK(mc_conn_poll(&sender)); CHECK(sender.tx.pos>0 && sender.tx.pos<sender.tx.len);
    int delivered=0;
    for (int i=0;i<1000 && delivered<16;i++) {
        CHECK(mc_conn_poll(&sender)); CHECK(mc_conn_poll(&receiver));
        while ((result=mc_conn_next(&receiver,&out))==1) {
            CHECK(out.len == 300001 && out.data[0] == 1 && memcmp(out.data+1,large,300000) == 0); delivered++;
        }
        CHECK(result == 0); if (delivered<16) mc_sleep_ms(1);
    }
    CHECK(delivered == 16 && sender.tx.len == 0); free(large);
    /* A peer may send its last packet and FIN in the same receive poll. */
    mc_buf_clear(&p); mc_put_varint(&p,2); mc_put_u8(&p,99); CHECK(mc_conn_send(&sender,&p));
    CHECK(mc_conn_poll(&sender)); CHECK(sender.tx.len == 0); mc_conn_close(&sender);
    result=0;
    for (int i=0;i<100 && result == 0;i++) { CHECK(mc_conn_poll(&receiver)); result=mc_conn_next(&receiver,&out); if (!result) mc_sleep_ms(1); }
    CHECK(result == 1 && out.len == 2 && out.data[0] == 2 && out.data[1] == 99);
    bool ended=false;
    for (int i=0;i<100 && !ended;i++) { ended=!mc_conn_poll(&receiver); if (!ended) mc_sleep_ms(1); }
    CHECK(ended && receiver.closed); mc_conn_close(&receiver); mc_socket_close(listener);
    mc_buf_free(&p); mc_buf_free(&out); mc_net_shutdown();
}

static void test_truncated_eof(void) {
    const uint8_t partials[][3]={{0x80,0,0},{3,0,99}};
    const size_t sizes[]={1,3};
    for (size_t i=0;i<2;i++) {
        mc_conn c; mc_conn_init(&c,MC_INVALID_SOCKET); mc_buf out; mc_buf_init(&out);
        mc_put_bytes(&c.rx,partials[i],sizes[i]); c.read_eof=true;
        CHECK(mc_conn_next(&c,&out) == -1 && c.closed && strstr(c.error,"Truncated") != NULL);
        mc_buf_free(&out); mc_conn_close(&c);
    }
}

static void test_half_close_retains_backlog_and_flushes_replies(void) {
    CHECK(mc_net_init()); char error[160]; mc_socket listener=mc_net_listen("127.0.0.1",0,error,sizeof(error));
    CHECK(listener != MC_INVALID_SOCKET); struct sockaddr_in addr;
#ifdef _WIN32
    int addr_size=sizeof(addr);
#else
    socklen_t addr_size=sizeof(addr);
#endif
    CHECK(getsockname(listener,(struct sockaddr*)&addr,&addr_size) == 0);
    mc_socket client=mc_net_connect("127.0.0.1",ntohs(addr.sin_port),error,sizeof(error)); CHECK(client != MC_INVALID_SOCKET);
    mc_socket peer=MC_INVALID_SOCKET;
    for (int i=0;i<100 && peer == MC_INVALID_SOCKET;i++) { peer=mc_net_accept(listener); if (peer == MC_INVALID_SOCKET) mc_sleep_ms(1); }
    CHECK(peer != MC_INVALID_SOCKET); mc_conn sender,receiver; mc_conn_init(&sender,client); mc_conn_init(&receiver,peer);
    mc_buf p,out; mc_buf_init(&p); mc_buf_init(&out);
    for (unsigned i=0;i<80;i++) {
        mc_buf_clear(&p); mc_put_varint(&p,0); mc_put_u8(&p,(uint8_t)i); CHECK(mc_conn_send(&sender,&p));
    }
    CHECK(mc_conn_poll(&sender) && sender.tx.len == 0);
#ifdef _WIN32
    CHECK(shutdown(sender.socket,SD_SEND) == 0);
#else
    CHECK(shutdown(sender.socket,SHUT_WR) == 0);
#endif
    for (int i=0;i<100 && !receiver.read_eof;i++) { CHECK(mc_conn_poll(&receiver)); if (!receiver.read_eof) mc_sleep_ms(1); }
    CHECK(receiver.read_eof && !receiver.closed && receiver.socket != MC_INVALID_SOCKET);
    /* Drain at most seven requests per application tick, with polls between ticks. */
    for (unsigned offset=0;offset<80;offset+=7) {
        CHECK(mc_conn_poll(&receiver));
        for (unsigned i=offset;i<offset+7 && i<80;i++) {
            CHECK(mc_conn_next(&receiver,&out) == 1 && mc_get_varint(&out) == 0 && mc_get_u8(&out) == i && !out.failed);
            mc_buf_clear(&p); mc_put_varint(&p,2); mc_put_u8(&p,(uint8_t)(255-i)); CHECK(mc_conn_send(&receiver,&p));
        }
    }
    CHECK(mc_conn_next(&receiver,&out) == 0 && !receiver.closed);
    CHECK(!mc_conn_poll(&receiver) && receiver.closed); /* Last replies are flushed before closure. */
    unsigned replies=0;
    for (int i=0;i<100 && replies<80;i++) {
        CHECK(mc_conn_poll(&sender)); int result;
        while ((result=mc_conn_next(&sender,&out)) == 1) {
            CHECK(mc_get_varint(&out) == 2 && mc_get_u8(&out) == 255-replies && !out.failed); replies++;
        }
        CHECK(result == 0); if (replies<80) mc_sleep_ms(1);
    }
    CHECK(replies == 80); mc_conn_close(&sender); mc_conn_close(&receiver); mc_socket_close(listener);
    mc_buf_free(&p); mc_buf_free(&out); mc_net_shutdown();
}

int main(void) {
    test_wire_values(); test_varints(); test_strings_and_bounds(); test_position(); test_uuid_and_json();
    test_frames(); test_invalid_frames(); test_frame_and_queue_limits(); test_loopback_transport();
    test_truncated_eof(); test_half_close_retains_backlog_and_flushes_replies();
    printf("protocol47: %u checks passed\n",checks); return 0;
}
