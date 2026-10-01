#include "client.h"
#include "renderer.h"
#include "block/block.h"
#include "crafting/crafting.h"
#include "network/play/client/C08PacketPlayerBlockPlacementValue.h"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

bool mc_client_inventory_ready(const mc_client *c) {
    return c->inventory_ready && (!c->window_id || c->window_ready);
}
unsigned mc_client_window_slots(const mc_client *c) { return mc_container_slot_count(&c->container); }
const mc_slot *mc_client_window_slot(const mc_client *c,int index) {
    return mc_container_const_get(&c->inventory,&c->container,index);
}

void mc_client_slot_name(const mc_slot *slot,char *output,size_t capacity) {
    if (!output || !capacity) return;
    mc_nbt_view root,display,name;
    if (slot && slot->item_id>=0 && mc_nbt_root(&slot->nbt,&root) &&
        mc_nbt_find(&root,"display",&display) && mc_nbt_find(&display,"Name",&name)) {
        if (mc_nbt_get_string(&name,output,capacity)) return;
        if (name.type==8 && name.size<=UINT16_MAX+2u) {
            char *full=malloc(name.size+1);
            if (full) {
                if (mc_nbt_get_string(&name,full,name.size+1)) {
                    size_t count=strlen(full); if (count>=capacity) count=capacity-1;
                    while (count && ((unsigned char)full[count]&0xc0u)==0x80u) --count;
                    memcpy(output,full,count); output[count]=0; free(full); return;
                }
                free(full);
            }
        }
    }
    snprintf(output,capacity,"%s",slot && slot->item_id>=0 ? mc_item_name(slot->item_id) : "Empty");
}

static void client_error(mc_client *c, const char *message) {
    if (!c->failed) {
        snprintf(c->status, sizeof(c->status), "%s", message);
        fprintf(stderr, "Client error: %s\n", c->status);
    }
    c->failed = true;
}

static bool send_packet(mc_client *c, mc_buf *packet) {
    bool ok = !packet->failed && mc_conn_send(&c->connection, packet);
    if (!ok) client_error(c, c->connection.error[0] ? c->connection.error : "Unable to send packet");
    mc_buf_free(packet);
    return ok;
}

static void start_packet(mc_buf *packet, int id) {
    mc_buf_init(packet);
    mc_put_varint(packet, id);
}

static void skip_string(mc_buf *b) {
    int32_t length = mc_get_varint(b);
    if (length < 0 || length > 32767 * 4 || (size_t)length > b->len - b->pos) b->failed = true;
    else b->pos += (size_t)length;
}

/* The HUD uses the text fields of protocol chat components, including extra[]. */
static void plain_chat(const char *json, char *output, size_t capacity) {
    size_t used = 0;
    const char *cursor = json;
    bool found = false;
    while ((cursor = strstr(cursor, "\"text\"")) != NULL) {
        cursor += 6;
        while (*cursor && *cursor != ':') ++cursor;
        if (*cursor) ++cursor;
        while (isspace((unsigned char)*cursor)) ++cursor;
        if (*cursor != '"') continue;
        ++cursor;
        found = true;
        while (*cursor && *cursor != '"') {
            unsigned char ch = (unsigned char)*cursor++;
            if (ch == '\\' && *cursor) {
                ch = (unsigned char)*cursor++;
                if (ch == 'n' || ch == 'r' || ch == 't') ch = ' ';
                else if (ch == 'u') {
                    unsigned value = 0;
                    bool valid = true;
                    for (int i = 0; i < 4; ++i) {
                        char digit = cursor[i];
                        if (!digit || !isxdigit((unsigned char)digit)) { valid = false; break; }
                        value = value * 16u + (unsigned)(isdigit((unsigned char)digit) ? digit - '0' : tolower((unsigned char)digit) - 'a' + 10);
                    }
                    if (valid) {
                        cursor += 4;
                        if (value < 128) ch = (unsigned char)value;
                        else {
                            if (value >= 0xd800 && value <= 0xdbff && cursor[0] == '\\' && cursor[1] == 'u') {
                                unsigned low = 0; bool paired = true;
                                for (int i = 0; i < 4; ++i) {
                                    char digit = cursor[2 + i];
                                    if (!digit || !isxdigit((unsigned char)digit)) { paired = false; break; }
                                    low = low * 16u + (unsigned)(isdigit((unsigned char)digit) ? digit - '0' : tolower((unsigned char)digit) - 'a' + 10);
                                }
                                if (paired && low >= 0xdc00 && low <= 0xdfff) { value = 0x10000u + ((value - 0xd800u) << 10) + low - 0xdc00u; cursor += 6; }
                            }
                            if (value >= 0xd800 && value <= 0xdfff) value = '?';
                            unsigned char encoded[4];
                            int count;
                            if (value < 128) { encoded[0] = (unsigned char)value; count = 1; }
                            else if (value < 2048) { encoded[0] = (unsigned char)(0xc0u | (value >> 6)); encoded[1] = (unsigned char)(0x80u | (value & 63u)); count = 2; }
                            else if (value < 65536) { encoded[0] = (unsigned char)(0xe0u | (value >> 12)); encoded[1] = (unsigned char)(0x80u | ((value >> 6) & 63u)); encoded[2] = (unsigned char)(0x80u | (value & 63u)); count = 3; }
                            else { encoded[0] = (unsigned char)(0xf0u | (value >> 18)); encoded[1] = (unsigned char)(0x80u | ((value >> 12) & 63u)); encoded[2] = (unsigned char)(0x80u | ((value >> 6) & 63u)); encoded[3] = (unsigned char)(0x80u | (value & 63u)); count = 4; }
                            if (used + (size_t)count < capacity) for (int i = 0; i < count; ++i) output[used++] = (char)encoded[i];
                            continue;
                        }
                    } else ch = '?';
                }
            }
            if (ch >= 128) {
                size_t width = ch < 0xe0 ? 2u : ch < 0xf0 ? 3u : 4u;
                if (strlen(cursor) < width - 1) break;
                if (used + width < capacity) { output[used++] = (char)ch; memcpy(output + used, cursor, width - 1); used += width - 1; }
                cursor += width - 1;
            } else if (ch >= 32 && used + 1 < capacity) output[used++] = (char)ch;
        }
        if (*cursor == '"') ++cursor;
    }
    if (!found) {
        cursor = json;
        if (*cursor == '"') ++cursor;
        while (*cursor && used + 1 < capacity) {
            unsigned char ch = (unsigned char)*cursor++;
            if (ch >= 32 && ch != '"') output[used++] = (char)ch;
        }
    }
    output[used] = '\0';
}

static bool read_chat(mc_buf *b, char *output, size_t capacity) {
    size_t start = b->pos;
    int32_t length = mc_get_varint(b);
    if (b->failed || length < 0 || length > 131068 || (size_t)length > b->len - b->pos) { b->failed = true; return false; }
    char *json = malloc((size_t)length + 1);
    if (!json) { b->failed = true; return false; }
    b->pos = start;
    if (!mc_get_string(b, json, (size_t)length + 1)) { free(json); return false; }
    plain_chat(json, output, capacity);
    free(json);
    return true;
}

static void json_space(const char **p) { while (**p==' ' || **p=='\n' || **p=='\r' || **p=='\t') ++*p; }
static bool json_string(const char **p) {
    if (*(*p)++!='"') return false;
    while (**p && **p!='"') {
        unsigned char ch=(unsigned char)*(*p)++;
        if (ch<32) return false;
        if (ch=='\\') {
            char escape=*(*p)++; if (!escape) return false;
            if (escape=='u') for (unsigned i=0;i<4;i++) { if (!isxdigit((unsigned char)**p)) return false; ++*p; }
            else if (!strchr("\"\\/bfnrt",escape)) return false;
        }
    }
    if (**p!='"') return false;
    ++*p; return true;
}
static bool json_value(const char **p,unsigned depth,unsigned *nodes) {
    if (depth>32 || ++*nodes>8192) return false;
    json_space(p);
    if (**p=='"') return json_string(p);
    if (**p=='{' || **p=='[') {
        char end=*(*p)++=='{' ? '}' : ']'; json_space(p);
        if (**p==end) { ++*p; return true; }
        do {
            if (end=='}') { if (!json_string(p)) return false; json_space(p); if (*(*p)++!=':') return false; }
            if (!json_value(p,depth+1,nodes)) return false;
            json_space(p); if (**p==end) { ++*p; return true; }
            if (*(*p)++!=',') return false;
            json_space(p);
        } while (**p);
        return false;
    }
    for (unsigned i=0;i<3;i++) {
        const char *literal=i==0 ? "true" : i==1 ? "false" : "null";
        size_t n=strlen(literal); if (!strncmp(*p,literal,n)) { *p+=n; return true; }
    }
    if (**p=='-') ++*p;
    if (**p=='0') ++*p;
    else { if (**p<'1' || **p>'9') return false; do { ++*p; } while (isdigit((unsigned char)**p)); }
    if (**p=='.') { ++*p; if (!isdigit((unsigned char)**p)) return false; do { ++*p; } while (isdigit((unsigned char)**p)); }
    if (**p=='e' || **p=='E') {
        ++*p; if (**p=='+' || **p=='-') ++*p;
        if (!isdigit((unsigned char)**p)) return false;
        do { ++*p; } while (isdigit((unsigned char)**p));
    }
    return true;
}
static bool read_window_title(mc_buf *b,char output[256]) {
    size_t start=b->pos; int32_t size=mc_get_varint(b);
    if (b->failed || size<0 || size>131068 || (size_t)size>b->len-b->pos) { b->failed=true; return false; }
    char *json=malloc((size_t)size+1); if (!json) { b->failed=true; return false; }
    b->pos=start; bool ok=mc_get_string(b,json,(size_t)size+1); unsigned nodes=0; const char *p=json;
    if (ok) { ok=json_value(&p,0,&nodes); if (ok) { json_space(&p); ok=!*p; } }
    if (ok) {
        plain_chat(json,output,256);
        if (!strstr(json,"\"text\"") && strstr(json,"\"translate\"")) snprintf(output,256,"Workbench");
    } else b->failed=true;
    free(json); return ok;
}

static void add_chat(mc_client *c, const char *text) {
    for (int i = 1; i < MC_CLIENT_CHAT_LINES; ++i) memcpy(c->chat[i - 1], c->chat[i], sizeof(c->chat[i]));
    snprintf(c->chat[MC_CLIENT_CHAT_LINES - 1], sizeof(c->chat[0]), "%s", text);
    if (c->chat_count < MC_CLIENT_CHAT_LINES) ++c->chat_count;
    printf("CHAT %s\n", text);
    fflush(stdout);
}

static mc_player_info *player_info(mc_client *c, const uint8_t uuid[16], bool create) {
    mc_player_info *empty = NULL;
    for (int i = 0; i < MC_CLIENT_PLAYERS; ++i) {
        if (c->player_info[i].used && memcmp(c->player_info[i].uuid, uuid, 16) == 0) return &c->player_info[i];
        if (!c->player_info[i].used && !empty) empty = &c->player_info[i];
    }
    if (create && empty) { empty->used = true; memcpy(empty->uuid, uuid, 16); return empty; }
    return NULL;
}

static mc_remote_player *remote_player(mc_client *c, int id, bool create) {
    mc_remote_player *empty = NULL;
    for (int i = 0; i < MC_CLIENT_PLAYERS; ++i) {
        if (c->players[i].active && c->players[i].id == id) return &c->players[i];
        if (!c->players[i].active && !empty) empty = &c->players[i];
    }
    if (create && empty) { memset(empty, 0, sizeof(*empty)); empty->active = true; empty->id = id; return empty; }
    return NULL;
}
static mc_client_item *client_item(mc_client *c,int32_t eid) {
    for (unsigned i=0;i<MC_MAX_ITEM_ENTITIES;i++) if (c->items[i].active && c->items[i].entity.eid==eid) return &c->items[i];
    return NULL;
}
static void remove_item(mc_client *c,mc_client_item *item) {
    if (!item || !item->active) return;
    c->item_nbt_bytes-=item->entity.item.nbt.size; mc_item_entity_free(&item->entity);
    memset(item,0,sizeof(*item));
}
static void clear_items(mc_client *c) {
    for (unsigned i=0;i<MC_MAX_ITEM_ENTITIES;i++) remove_item(c,&c->items[i]);
}
static void unload_items(mc_client *c,int x,int z) {
    for (unsigned i=0;i<MC_MAX_ITEM_ENTITIES;i++) {
        mc_client_item *item=&c->items[i];
        if (item->active && mc_floor_div16((int)floor(item->entity.x))==x && mc_floor_div16((int)floor(item->entity.z))==z) remove_item(c,item);
    }
}
static void receive_object(mc_client *c,mc_buf *b) {
    int32_t eid=mc_get_varint(b); unsigned type=mc_get_u8(b);
    int32_t x=mc_get_i32(b),y=mc_get_i32(b),z=mc_get_i32(b);
    (void)mc_get_u8(b); (void)mc_get_u8(b); int32_t data=mc_get_i32(b);
    double vx=0,vy=0,vz=0;
    if (data>0) { vx=mc_get_i16(b)/8000.0; vy=mc_get_i16(b)/8000.0; vz=mc_get_i16(b)/8000.0; }
    if (b->failed || b->pos!=b->len) { b->failed=true; return; }
    if (type!=2) return;
    if (fabs(x/32.0)>30000000 || fabs(y/32.0)>30000000 || fabs(z/32.0)>30000000) { b->failed=true; return; }
    mc_client_item *item=client_item(c,eid);
    if (!item) for (unsigned i=0;i<MC_MAX_ITEM_ENTITIES;i++) if (!c->items[i].active) { item=&c->items[i]; break; }
    if (!item) { b->failed=true; return; }
    remove_item(c,item); memset(item,0,sizeof(*item)); mc_item_entity_init(&item->entity);
    item->active=true; item->entity.eid=eid; item->server_x=x; item->server_y=y; item->server_z=z;
    item->entity.x=x/32.0; item->entity.y=y/32.0; item->entity.z=z/32.0;
    item->entity.vx=vx; item->entity.vy=vy; item->entity.vz=vz; ++c->item_spawns;
}
static void receive_metadata(mc_client *c,mc_buf *b) {
    int32_t eid=mc_get_varint(b); mc_slot next; mc_slot_init(&next); bool has_slot=false,ended=false;
    while (!b->failed && b->pos<b->len) {
        unsigned header=mc_get_u8(b); if (header==0x7f) { ended=true; break; }
        unsigned type=header>>5,index=header&31;
        if (index==10 && type!=5 && client_item(c,eid)) { b->failed=true; break; }
        switch (type) {
            case 0: (void)mc_get_u8(b); break;
            case 1: (void)mc_get_i16(b); break;
            case 2: (void)mc_get_i32(b); break;
            case 3: (void)mc_get_f32(b); break;
            case 4: skip_string(b); break;
            case 5: {
                mc_slot value; mc_slot_init(&value); mc_slot_read(b,&value);
                if (!b->failed && index==10) { mc_slot_free(&next); next=value; mc_slot_init(&value); has_slot=true; }
                mc_slot_free(&value); break;
            }
            case 6: for (unsigned i=0;i<3;i++) (void)mc_get_i32(b); break;
            case 7: for (unsigned i=0;i<3;i++) (void)mc_get_f32(b); break;
        }
    }
    if (!ended || b->pos!=b->len) b->failed=true;
    mc_client_item *item=client_item(c,eid);
    if (!b->failed && item && has_slot) {
        size_t total=c->item_nbt_bytes-item->entity.item.nbt.size;
        if (next.nbt.size>MC_NBT_MAX_BYTES-total) b->failed=true;
        else {
            c->item_nbt_bytes=total+next.nbt.size; mc_slot_free(&item->entity.item); item->entity.item=next; mc_slot_init(&next);
            item->metadata_ready=item->entity.item.item_id>=0; ++c->item_metadata;
        }
    }
    mc_slot_free(&next);
}

static mc_chunk *client_chunk(mc_client *c, int x, int z) {
    mc_chunk *chunk = mc_world_chunk(&c->world, x, z, false);
    if (chunk) return chunk;
    if (c->world.count == MC_MAX_CHUNKS) {
        int farthest = 0;
        double max_distance = -1;
        for (int i = 0; i < c->world.count; ++i) {
            double dx = c->world.chunks[i].x * 16.0 + 8 - c->x;
            double dz = c->world.chunks[i].z * 16.0 + 8 - c->z;
            double distance = dx * dx + dz * dz;
            if (distance > max_distance) { farthest = i; max_distance = distance; }
        }
        unload_items(c,c->world.chunks[farthest].x,c->world.chunks[farthest].z);
        mc_world_unload(&c->world, c->world.chunks[farthest].x, c->world.chunks[farthest].z);
    }
    return mc_world_chunk(&c->world, x, z, true);
}

static unsigned section_count(uint16_t mask) {
    unsigned result = 0;
    for (; mask; mask >>= 1) result += mask & 1u;
    return result;
}

static size_t chunk_size(uint16_t mask, bool skylight, bool full) {
    return section_count(mask) * (8192u + 2048u + (skylight ? 2048u : 0u)) + (full ? 256u : 0u);
}

static bool receive_chunk(mc_client *c, int x, int z, uint16_t mask, bool full,
                          bool skylight, const uint8_t *data, size_t length) {
    if (x < -1875000 || x >= 1875000 || z < -1875000 || z >= 1875000) return false;
    if (full && mask == 0) {
        if (length != 0 && length != 256) return false;
        unload_items(c,x,z); mc_world_unload(&c->world, x, z); return true;
    }
    if (length != chunk_size(mask, skylight, full)) return false;
    mc_chunk *chunk = client_chunk(c, x, z);
    if (!chunk) return false;
    if (full) memset(chunk->blocks, 0, MC_CHUNK_BLOCKS * sizeof(*chunk->blocks));
    size_t offset = 0;
    for (unsigned section = 0; section < 16; ++section) {
        if (!(mask & (1u << section))) continue;
        for (unsigned index = 0; index < 4096; ++index) {
            chunk->blocks[section * 4096u + index] = (uint16_t)(data[offset] | ((uint16_t)data[offset + 1] << 8));
            offset += 2;
        }
    }
    ++chunk->revision;
    ++c->chunks_received;
    return true;
}

static void send_movement(mc_client *c) {
    mc_buf packet;
    start_packet(&packet, 0x06);
    mc_put_f64(&packet, c->x); mc_put_f64(&packet, c->y); mc_put_f64(&packet, c->z);
    mc_put_f32(&packet, c->yaw); mc_put_f32(&packet, c->pitch);
    mc_put_u8(&packet, c->on_ground ? 1 : 0);
    send_packet(c, &packet);
    c->last_move_ms = mc_time_ms();
}

static bool correct_position(mc_client *c, double x, double y, double z, float yaw, float pitch, uint8_t flags) {
    if ((flags & ~31u) || !isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(yaw) || !isfinite(pitch)) return false;
    x += (flags & 1) ? c->x : 0; y += (flags & 2) ? c->y : 0; z += (flags & 4) ? c->z : 0;
    double final_yaw = (double)yaw + ((flags & 8) ? c->yaw : 0);
    double final_pitch = (double)pitch + ((flags & 16) ? c->pitch : 0);
    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(final_yaw) || !isfinite(final_pitch) ||
        x < -30000000 || x >= 30000000 || z < -30000000 || z >= 30000000 ||
        y < -1000000 || y > 1000000 || final_pitch < -90 || final_pitch > 90) return false;
    c->x = x; c->y = y; c->z = z; c->yaw = (float)fmod(final_yaw, 360.0); c->pitch = (float)final_pitch;
    c->positioned = true; c->velocity_y = 0; c->on_ground = false;
    return true;
}

static void send_abilities(mc_client *c) {
    mc_buf packet;
    start_packet(&packet, 0x13);
    mc_put_u8(&packet, (uint8_t)((c->gamemode == 1 ? 9 : 0) | (c->can_fly ? 4 : 0) | (c->flying ? 2 : 0)));
    mc_put_f32(&packet, 0.05f); mc_put_f32(&packet, 0.1f);
    send_packet(c, &packet);
}

static void send_slot(mc_client *c, int slot) {
    mc_buf packet;
    c->selected = slot;
    start_packet(&packet, 0x09); mc_put_i16(&packet, (int16_t)slot); send_packet(c, &packet);
}

static void configure_player(mc_client *c) {
    mc_buf packet;
    start_packet(&packet, 0x15);
    mc_put_string(&packet, "en_US"); mc_put_u8(&packet, 6); mc_put_u8(&packet, 0);
    mc_put_u8(&packet, 1); mc_put_u8(&packet, 127); send_packet(c, &packet);
    start_packet(&packet, 0x17);
    mc_put_string(&packet, "MC|Brand"); mc_put_string(&packet, "C919 native"); send_packet(c, &packet);
    if (c->gamemode == 1) c->can_fly = true;
    send_slot(c, c->selected);
}

static void cancel_inventory_actions(mc_client *c) {
    c->inventory_queue_count=0; c->inventory_queue_index=0; c->inventory_close_requested=false;
    c->inventory_pending=false; c->inventory_sync=false; c->inventory_sync_slots=false; c->inventory_sync_cursor=false;
}
static void reset_window(mc_client *c) {
    cancel_inventory_actions(c);
    mc_container_free(&c->container); mc_container_init(&c->container,MC_CONTAINER_PLAYER);
    mc_container_free(&c->container_authoritative); mc_container_init(&c->container_authoritative,MC_CONTAINER_PLAYER);
    c->window_id=0; c->window_ready=false; ++c->window_generation; c->window_title[0]=0;
}
static void receive_open_window(mc_client *c,mc_buf *b) {
    unsigned window=mc_get_u8(b); char type[129],title[256];
    if (!mc_get_string(b,type,sizeof(type))) return;
    unsigned units=0; for (const unsigned char *p=(const unsigned char *)type;*p;p++) if ((*p&0xc0u)!=0x80u) units+=*p>=0xf0u ? 2u : 1u;
    if (units>32 || !read_window_title(b,title)) { b->failed=true; return; }
    unsigned count=mc_get_u8(b); if (!strcmp(type,"EntityHorse")) (void)mc_get_i32(b);
    if (b->failed || b->pos!=b->len) { b->failed=true; return; }
    bool workbench=window && !strcmp(type,"minecraft:crafting_table") && count==0;
    if (!workbench) {
        mc_buf reply; start_packet(&reply,0x0d); mc_put_u8(&reply,(uint8_t)window);
        if (send_packet(c,&reply)) {
            reset_window(c); c->inventory_open=false; c->creative_open=false;
            snprintf(c->inventory_status,sizeof(c->inventory_status),"This external container is unavailable; its window was closed");
        }
        return;
    }
    reset_window(c); c->window_id=(uint8_t)window;
    mc_container_init(&c->container,MC_CONTAINER_WORKBENCH); mc_container_init(&c->container_authoritative,MC_CONTAINER_WORKBENCH);
    snprintf(c->window_title,sizeof(c->window_title),"%s",title[0] ? title : "Workbench");
    c->inventory_open=true; c->creative_open=false;
    snprintf(c->inventory_status,sizeof(c->inventory_status),"Waiting for workbench contents");
}
static mc_crafting_context client_crafting_context(mc_client *c) {
    mc_crafting_context context={0}; context.creative=c->gamemode==1; context.authoritative=false;
    context.player_x=c->x; context.player_z=c->z; context.dimension=c->dimension;
    context.maps=&c->maps;
    return context;
}
static size_t inventory_nbt_bytes(const mc_inventory *player,const mc_container *container);
static void inventory_click(mc_client *c,int slot,int button,int mode) {
    if (!mc_client_inventory_ready(c) || c->inventory_pending || c->inventory_sync) {
        snprintf(c->inventory_status,sizeof(c->inventory_status),"Waiting for server inventory synchronization"); return;
    }
    if ((slot!= -999 && (slot<0 || (unsigned)slot>=mc_client_window_slots(c))) ||
        (mode==3 && c->gamemode!=1) || (mode==5 && button>=8 && c->gamemode!=1)) {
        snprintf(c->inventory_status,sizeof(c->inventory_status),"This inventory action is unavailable in the current game mode"); return;
    }
    mc_inventory predicted; mc_inventory_init(&predicted);
    mc_container predicted_container; mc_container_init(&predicted_container,c->container.kind);
    mc_crafting_context context=client_crafting_context(c);
    mc_slot returned; mc_slot_init(&returned); mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    if (!mc_inventory_copy(&predicted,&c->inventory) || !mc_container_copy(&predicted_container,&c->container) ||
        !mc_container_click(&predicted,&predicted_container,&context,slot,button,mode,&returned,&effects) ||
        inventory_nbt_bytes(&predicted,&predicted_container)>MC_MAX_PACKET) {
        snprintf(c->inventory_status,sizeof(c->inventory_status),"This inventory action is unavailable"); goto done;
    }
    c->inventory_action=c->next_inventory_action;
    c->next_inventory_action=(int16_t)((uint16_t)c->next_inventory_action+1u);
    mc_buf packet; start_packet(&packet,0x0e); mc_put_u8(&packet,c->window_id);
    mc_put_i16(&packet,(int16_t)slot); mc_put_u8(&packet,(uint8_t)button);
    mc_put_i16(&packet,c->inventory_action); mc_put_u8(&packet,(uint8_t)mode); mc_slot_write(&packet,&returned);
    if (send_packet(c,&packet)) {
        mc_inventory_free(&c->inventory); c->inventory=predicted; mc_inventory_init(&predicted);
        mc_container_free(&c->container); c->container=predicted_container; mc_container_init(&predicted_container,c->container.kind);
        c->inventory_pending_window=c->window_id; c->inventory_pending_generation=c->window_generation;
        c->inventory_pending=true; c->inventory_pending_ms=mc_time_ms();
        snprintf(c->inventory_status,sizeof(c->inventory_status),"Waiting for transaction %d",c->inventory_action);
    }
done:
    mc_inventory_free(&predicted); mc_container_free(&predicted_container); mc_slot_free(&returned); mc_crafting_effects_free(&effects);
}

static void creative_pick(mc_client *c,unsigned index) {
    if (c->window_id || c->gamemode!=1 || !mc_client_inventory_ready(c) || c->inventory_pending || c->inventory_sync || c->inventory.cursor.item_id>=0) {
        snprintf(c->inventory_status,sizeof(c->inventory_status),"Creative selection needs synchronized inventory and an empty cursor"); return;
    }
    int16_t id,damage;
    if (!mc_item_creative_at(index,&id,&damage)) return;
    mc_slot item; mc_slot_init(&item);
    if (!mc_slot_set(&item,id,(uint8_t)mc_item_stack_limit(id),damage)) { client_error(c,"Cannot construct creative slot"); return; }
    mc_buf packet; start_packet(&packet,0x10); mc_put_i16(&packet,(int16_t)(MC_HOTBAR_START+c->selected)); mc_slot_write(&packet,&item);
    if (send_packet(c,&packet)) {
        int slot=MC_HOTBAR_START+c->selected;
        if (!mc_slot_copy(&c->inventory.slots[slot],&item) || !mc_slot_copy(&c->inventory_authoritative.slots[slot],&item)) client_error(c,"Cannot retain creative inventory slot");
        snprintf(c->inventory_status,sizeof(c->inventory_status),"Added %s to hotbar %d",mc_item_name(id),c->selected+1);
    }
    mc_slot_free(&item);
}

static void close_inventory(mc_client *c,bool notify_server) {
    if (!notify_server) {
        /* A server close clears the shared cursor before closing the GUI.
           Only the ordinary player GUI invokes its 2x2 container cleanup. */
        if (!c->window_id && c->inventory_open && !c->creative_open) for (int i=0;i<=4;i++) {
            mc_slot_free(&c->inventory.slots[i]); mc_slot_free(&c->inventory_authoritative.slots[i]);
        }
        mc_slot_free(&c->inventory.cursor); mc_slot_free(&c->inventory_authoritative.cursor);
        c->inventory.drag_active=false; c->inventory.drag_mode=0; c->inventory.drag_slots=0;
        c->inventory_authoritative.drag_active=false; c->inventory_authoritative.drag_mode=0; c->inventory_authoritative.drag_slots=0;
        reset_window(c);
        c->inventory_open=false; c->creative_open=false; c->inventory_close_requested=false;
        return;
    }
    if (c->inventory_pending || c->inventory_sync || c->inventory_queue_index<c->inventory_queue_count) {
        c->inventory_close_requested=true;
        snprintf(c->inventory_status,sizeof(c->inventory_status),"Closing after the server confirms inventory synchronization"); return;
    }
    mc_inventory next,authoritative; mc_inventory_init(&next); mc_inventory_init(&authoritative);
    mc_container next_container; mc_container_init(&next_container,c->container.kind);
    mc_crafting_effects effects; mc_crafting_effects_init(&effects);
    if (!mc_inventory_copy(&next,&c->inventory) || !mc_container_copy(&next_container,&c->container) ||
        !mc_container_close(&next,&next_container,&effects) || !mc_inventory_copy(&authoritative,&next)) {
        client_error(c,"Cannot retain closed inventory state"); goto done;
    }
    mc_buf packet; start_packet(&packet,0x0d); mc_put_u8(&packet,c->window_id);
    if (send_packet(c,&packet)) {
        mc_inventory_free(&c->inventory); c->inventory=next; mc_inventory_init(&next);
        mc_inventory_free(&c->inventory_authoritative); c->inventory_authoritative=authoritative; mc_inventory_init(&authoritative);
        reset_window(c);
        c->inventory_open=false; c->creative_open=false; c->inventory_close_requested=false;
    }
done:
    mc_inventory_free(&next); mc_inventory_free(&authoritative); mc_container_free(&next_container); mc_crafting_effects_free(&effects);
}
static void drop_held_item(mc_client *c,bool all) {
    if (!c->inventory_ready || c->inventory_pending || c->inventory_sync) return;
    mc_slot *held=&c->inventory.slots[36+c->selected]; if (held->item_id<0) return;
    mc_slot next,authoritative; mc_slot_init(&next); mc_slot_init(&authoritative);
    if (!mc_slot_copy(&next,held)) { client_error(c,"Cannot retain dropped inventory state"); return; }
    if (all || next.count==1) mc_slot_free(&next); else --next.count;
    if (!mc_slot_copy(&authoritative,&next)) { client_error(c,"Cannot retain dropped inventory state"); mc_slot_free(&next); return; }
    mc_buf packet; start_packet(&packet,0x07); mc_put_varint(&packet,all ? 3 : 4); mc_put_position(&packet,0,0,0); mc_put_u8(&packet,0);
    if (send_packet(c,&packet)) {
        mc_slot_free(held); *held=next; mc_slot_init(&next);
        mc_slot *saved=&c->inventory_authoritative.slots[36+c->selected]; mc_slot_free(saved); *saved=authoritative; mc_slot_init(&authoritative);
    }
    mc_slot_free(&next); mc_slot_free(&authoritative);
}

static size_t inventory_nbt_bytes(const mc_inventory *player,const mc_container *container) {
    size_t bytes=player->cursor.nbt.size;
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) bytes+=player->slots[i].nbt.size;
    if (container->kind==MC_CONTAINER_WORKBENCH) for (int i=0;i<10;i++) bytes+=container->slots[i].nbt.size;
    return bytes;
}
static void receive_inventory(mc_client *c,mc_buf *b) {
    unsigned window=mc_get_u8(b); int count=mc_get_i16(b);
    bool matching=window==0 || (c->window_id && window==c->window_id);
    unsigned expected=window==0 ? MC_PLAYER_INVENTORY_SIZE : mc_client_window_slots(c);
    if (b->failed || count<0 || (matching && (unsigned)count!=expected)) { b->failed=true; return; }
    mc_inventory next,authoritative; mc_inventory_init(&next); mc_inventory_init(&authoritative);
    mc_container next_container,saved; mc_container_init(&next_container,c->container.kind); mc_container_init(&saved,c->container.kind);
    if (matching && (!mc_inventory_copy(&next,&c->inventory) || !mc_container_copy(&next_container,&c->container))) b->failed=true;
    for (int i=0;i<count && !b->failed;i++) {
        mc_slot discard; mc_slot_init(&discard);
        mc_slot *slot=!matching ? &discard : window==0 ? &next.slots[i] : mc_container_get(&next,&next_container,i);
        if (!slot || !mc_slot_read(b,slot)) b->failed=true;
        mc_slot_free(&discard);
    }
    if (!b->failed && b->pos!=b->len) b->failed=true;
    if (!b->failed && matching) {
        bool sync_window=c->inventory_sync && window==c->inventory_pending_window;
        if (sync_window) {
            next.drag_active=false; next.drag_mode=0; next.drag_slots=0;
            mc_container_reset_drag(&next_container);
        }
        const mc_slot *cursor=c->inventory_pending ? &c->inventory.cursor : &c->inventory_authoritative.cursor;
        const mc_container *saved_source=window==0 && c->window_id ? &c->container_authoritative : &next_container;
        if (!mc_slot_copy(&next.cursor,cursor) || inventory_nbt_bytes(&next,&next_container)>MC_MAX_PACKET ||
            !mc_inventory_copy(&authoritative,&next) ||
            !mc_slot_copy(&authoritative.cursor,&c->inventory_authoritative.cursor) ||
            !mc_container_copy(&saved,saved_source) || inventory_nbt_bytes(&authoritative,&saved)>MC_MAX_PACKET) b->failed=true;
        else {
            mc_inventory_free(&c->inventory); c->inventory=next; mc_inventory_init(&next);
            mc_inventory_free(&c->inventory_authoritative); c->inventory_authoritative=authoritative; mc_inventory_init(&authoritative);
            mc_container_free(&c->container); c->container=next_container; mc_container_init(&next_container,c->container.kind);
            mc_container_free(&c->container_authoritative); c->container_authoritative=saved; mc_container_init(&saved,c->container.kind);
            if (!window) c->inventory_ready=true; else c->window_ready=true;
            ++c->inventory_packets;
            if (sync_window) {
                c->inventory_sync_slots=true;
                if (c->inventory_sync_cursor) c->inventory_sync=false;
            }
            snprintf(c->inventory_status,sizeof(c->inventory_status),"%s",c->inventory_sync ? "Waiting for authoritative cursor synchronization" : "Inventory synchronized");
        }
    }
    mc_inventory_free(&next); mc_inventory_free(&authoritative); mc_container_free(&next_container); mc_container_free(&saved);
}

static void receive_slot(mc_client *c,mc_buf *b) {
    int window=(int8_t)mc_get_u8(b),index=mc_get_i16(b);
    mc_slot item; mc_slot_init(&item); mc_slot_read(b,&item);
    if (!b->failed && b->pos!=b->len) b->failed=true;
    mc_slot *current=NULL,*authoritative=NULL;
    if (window==-1 && index==-1) { current=&c->inventory.cursor; authoritative=&c->inventory_authoritative.cursor; }
    else if (window==-2 && index>=0 && index<40) {
        if (window==-2) index=index<9 ? MC_HOTBAR_START+index : index<36 ? index : 44-index;
        current=&c->inventory.slots[index]; authoritative=&c->inventory_authoritative.slots[index];
    } else if (window==0 && index>=36 && index<45) {
        current=&c->inventory.slots[index]; authoritative=&c->inventory_authoritative.slots[index];
    } else if ((uint8_t)window==c->window_id && index>=0 && (unsigned)index<mc_client_window_slots(c)) {
        current=mc_container_get(&c->inventory,&c->container,index);
        authoritative=mc_container_get(&c->inventory_authoritative,&c->container_authoritative,index);
    } else if (window==-1 || window==-2 || (uint8_t)window==c->window_id) b->failed=true;
    if (!b->failed && current) {
        if (inventory_nbt_bytes(&c->inventory,&c->container)-current->nbt.size+item.nbt.size>MC_MAX_PACKET ||
            inventory_nbt_bytes(&c->inventory_authoritative,&c->container_authoritative)-authoritative->nbt.size+item.nbt.size>MC_MAX_PACKET ||
            !mc_slot_copy(current,&item)) b->failed=true;
        else {
            mc_slot_free(authoritative); *authoritative=item; mc_slot_init(&item); ++c->inventory_packets;
            if (window==-1 && c->inventory_sync) {
                c->inventory_sync_cursor=true;
                if (c->inventory_sync_slots) { c->inventory_sync=false; snprintf(c->inventory_status,sizeof(c->inventory_status),"Inventory synchronized"); }
            }
        }
    }
    mc_slot_free(&item);
}

static void receive_transaction(mc_client *c,mc_buf *b) {
    unsigned window=mc_get_u8(b); int16_t action=mc_get_i16(b); unsigned accepted=mc_get_u8(b);
    if (b->failed || accepted>1 || b->pos!=b->len) { b->failed=true; return; }
    if (window!=0 && window!=c->window_id) return;
    if (!accepted) {
        mc_buf reply; start_packet(&reply,0x0f); mc_put_u8(&reply,(uint8_t)window); mc_put_i16(&reply,action); mc_put_u8(&reply,1); send_packet(c,&reply);
    }
    if (!c->inventory_pending || action!=c->inventory_action || window!=c->inventory_pending_window ||
        c->inventory_pending_generation!=c->window_generation) return;
    c->inventory_pending=false;
    mc_inventory copied; mc_inventory_init(&copied); mc_container copied_container; mc_container_init(&copied_container,c->container.kind);
    bool copied_ok=mc_inventory_copy(&copied,accepted ? &c->inventory : &c->inventory_authoritative) &&
        mc_container_copy(&copied_container,accepted ? &c->container : &c->container_authoritative);
    if (!copied_ok) b->failed=true;
    if (accepted) {
        if (copied_ok) {
            mc_inventory_free(&c->inventory_authoritative); c->inventory_authoritative=copied; mc_inventory_init(&copied);
            mc_container_free(&c->container_authoritative); c->container_authoritative=copied_container; mc_container_init(&copied_container,c->container.kind);
        }
        snprintf(c->inventory_status,sizeof(c->inventory_status),"Inventory transaction accepted");
    } else {
        ++c->inventory_rejections; c->inventory_sync=true; c->inventory_sync_slots=false; c->inventory_sync_cursor=false;
        c->inventory_queue_count=0; c->inventory_queue_index=0;
        if (copied_ok) {
            mc_inventory_free(&c->inventory); c->inventory=copied; mc_inventory_init(&copied);
            mc_container_free(&c->container); c->container=copied_container; mc_container_init(&copied_container,c->container.kind);
        }
        snprintf(c->inventory_status,sizeof(c->inventory_status),"Server rejected action; waiting for inventory resynchronization");
    }
    mc_inventory_free(&copied); mc_container_free(&copied_container);
}

static void handle_player_info(mc_client *c, mc_buf *b) {
    int action = mc_get_varint(b), count = mc_get_varint(b);
    if (action < 0 || action > 4 || count < 0 || count > 4096) { b->failed = true; return; }
    for (int i = 0; i < count && !b->failed; ++i) {
        uint8_t uuid[16];
        if (!mc_get_bytes(b, uuid, sizeof(uuid))) break;
        mc_player_info *info = player_info(c, uuid, action == 0);
        if (action == 0) {
            char name[17];
            mc_get_string(b, name, sizeof(name));
            int properties = mc_get_varint(b);
            if (properties < 0 || properties > 64) { b->failed = true; break; }
            for (int p = 0; p < properties && !b->failed; ++p) {
                skip_string(b); skip_string(b); if (mc_get_u8(b)) skip_string(b);
            }
            (void)mc_get_varint(b); (void)mc_get_varint(b);
            if (mc_get_u8(b)) skip_string(b);
            if (info && !b->failed) {
                snprintf(info->name, sizeof(info->name), "%s", name);
                for (int p = 0; p < MC_CLIENT_PLAYERS; ++p)
                    if (c->players[p].active && memcmp(c->players[p].uuid, uuid, 16) == 0) snprintf(c->players[p].name, sizeof(c->players[p].name), "%s", name);
            }
        } else if (action == 1 || action == 2) (void)mc_get_varint(b);
        else if (action == 3) { if (mc_get_u8(b)) skip_string(b); }
        else if (info) info->used = false;
    }
}

static void handle_packet(mc_client *c, mc_buf *b) {
    int id = mc_get_varint(b);
    if (b->failed) return;
    if (c->state == 0) {
        if (id == 0x00) {
            char reason[256];
            if (read_chat(b, reason, sizeof(reason))) client_error(c, reason);
        } else if (id == 0x01) client_error(c, "This server requires online authentication/encryption. Use an offline-mode protocol 47 server.");
        else if (id == 0x02) {
            char uuid[37], name[17];
            mc_get_string(b, uuid, sizeof(uuid)); mc_get_string(b, name, sizeof(name));
            if (!b->failed) { c->state = 1; snprintf(c->status, sizeof(c->status), "Login accepted; waiting for world"); }
        } else if (id == 0x03) {
            int threshold = mc_get_varint(b);
            if (threshold < 0 || threshold > (int)MC_MAX_PACKET) b->failed = true;
            else c->connection.compression_threshold = threshold;
        } else b->failed = true;
        return;
    }

    mc_buf reply;
    switch (id) {
        case 0x00: {
            int keepalive = mc_get_varint(b);
            if (!b->failed) { start_packet(&reply, 0x00); mc_put_varint(&reply, keepalive); send_packet(c, &reply); }
            break;
        }
        case 0x01: {
            c->entity_id = mc_get_i32(b); c->gamemode = mc_get_u8(b) & 7;
            c->dimension = (int8_t)mc_get_u8(b); (void)mc_get_u8(b); (void)mc_get_u8(b);
            skip_string(b); (void)mc_get_u8(b);
            if (!b->failed) {
                c->joined = true;
                snprintf(c->status, sizeof(c->status), "Connected; loading terrain");
                printf("JOIN entity=%d gamemode=%d dimension=%d\n", c->entity_id, c->gamemode, c->dimension);
                configure_player(c);
            }
            break;
        }
        case 0x02: {
            char text[256];
            if (read_chat(b, text, sizeof(text))) { (void)mc_get_u8(b); add_chat(c, text); }
            break;
        }
        case 0x06: {
            float health = mc_get_f32(b); (void)mc_get_varint(b); (void)mc_get_f32(b);
            if (!b->failed && health <= 0) { start_packet(&reply, 0x16); mc_put_varint(&reply, 0); send_packet(c, &reply); }
            break;
        }
        case 0x07:
            c->dimension = mc_get_i32(b); (void)mc_get_u8(b); c->gamemode = mc_get_u8(b); skip_string(b);
            if (!b->failed) { close_inventory(c,false); clear_items(c); mc_maps_free(&c->maps); mc_maps_init(&c->maps); mc_world_free(&c->world); mc_world_init(&c->world, 0); memset(c->players, 0, sizeof(c->players)); c->positioned = false; c->velocity_y = 0; }
            break;
        case 0x08: {
            double x = mc_get_f64(b), y = mc_get_f64(b), z = mc_get_f64(b);
            float yaw = mc_get_f32(b), pitch = mc_get_f32(b); uint8_t flags = mc_get_u8(b);
            if (!b->failed && !correct_position(c, x, y, z, yaw, pitch, flags)) b->failed = true;
            if (!b->failed) send_movement(c);
            break;
        }
        case 0x09: {
            int slot = mc_get_u8(b); if (slot < 9) c->selected = slot;
            break;
        }
        case 0x0c: {
            int entity = mc_get_varint(b); uint8_t uuid[16]; mc_get_bytes(b, uuid, sizeof(uuid));
            double x = mc_get_i32(b) / 32.0, y = mc_get_i32(b) / 32.0, z = mc_get_i32(b) / 32.0;
            float yaw = mc_get_u8(b) * (360.0f / 256.0f), pitch = mc_get_u8(b) * (360.0f / 256.0f); (void)mc_get_i16(b);
            if (!b->failed) {
                remove_item(c,client_item(c,entity));
                mc_remote_player *p = remote_player(c, entity, true);
                if (p) {
                    memcpy(p->uuid, uuid, sizeof(uuid)); p->x = x; p->y = y; p->z = z; p->yaw = yaw; p->pitch = pitch;
                    mc_player_info *info = player_info(c, uuid, false);
                    snprintf(p->name, sizeof(p->name), "%s", info ? info->name : "Player");
                }
            }
            break;
        }
        case 0x0d: {
            int32_t entity=mc_get_varint(b); (void)mc_get_varint(b);
            if (b->failed || b->pos!=b->len) { b->failed=true; break; }
            mc_client_item *item=client_item(c,entity);
            if (item) { remove_item(c,item); ++c->item_collects; }
            break;
        }
        case 0x0e: receive_object(c,b); break;
        case 0x12: {
            int32_t entity=mc_get_varint(b); double vx=mc_get_i16(b)/8000.0,vy=mc_get_i16(b)/8000.0,vz=mc_get_i16(b)/8000.0;
            if (b->failed || b->pos!=b->len) { b->failed=true; break; }
            mc_client_item *item=client_item(c,entity);
            if (item) { item->entity.vx=vx; item->entity.vy=vy; item->entity.vz=vz; }
            break;
        }
        case 0x13: {
            int count = mc_get_varint(b);
            if (count < 0 || count > 4096) { b->failed = true; break; }
            int32_t ids[4096]; for (int i=0;i<count;i++) ids[i]=mc_get_varint(b);
            if (b->failed || b->pos!=b->len) { b->failed=true; break; }
            for (int i=0;i<count;i++) { mc_remote_player *p=remote_player(c,ids[i],false); if (p) p->active=false; remove_item(c,client_item(c,ids[i])); }
            break;
        }
        case 0x14: {
            int32_t eid=mc_get_varint(b); unsigned ground=mc_get_u8(b);
            if (b->failed || b->pos!=b->len || ground>1) { b->failed=true; break; }
            mc_client_item *item=client_item(c,eid); if (item) item->entity.on_ground=ground!=0;
            break;
        }
        case 0x15: case 0x16: case 0x17: case 0x18: {
            int entity = mc_get_varint(b); mc_remote_player *p = remote_player(c, entity, false);
            mc_client_item *item=client_item(c,entity);
            double x = 0, y = 0, z = 0; float yaw = 0, pitch = 0;
            if (id == 0x18) { x = mc_get_i32(b) / 32.0; y = mc_get_i32(b) / 32.0; z = mc_get_i32(b) / 32.0; }
            else if (id != 0x16) { x = (int8_t)mc_get_u8(b) / 32.0; y = (int8_t)mc_get_u8(b) / 32.0; z = (int8_t)mc_get_u8(b) / 32.0; }
            if (id != 0x15) { yaw = mc_get_u8(b) * (360.0f / 256.0f); pitch = mc_get_u8(b) * (360.0f / 256.0f); }
            unsigned ground=mc_get_u8(b);
            if (b->failed || b->pos!=b->len || ground>1) { b->failed=true; break; }
            if (!b->failed && p) {
                if (id == 0x18) { p->x = x; p->y = y; p->z = z; }
                else if (id != 0x16) { p->x += x; p->y += y; p->z += z; }
                if (id != 0x15) { p->yaw = yaw; p->pitch = pitch; }
            }
            if (item) {
                double next_x=id==0x18 ? x : item->server_x/32.0+x;
                double next_y=id==0x18 ? y : item->server_y/32.0+y;
                double next_z=id==0x18 ? z : item->server_z/32.0+z;
                if (fabs(next_x)>30000000 || fabs(next_y)>30000000 || fabs(next_z)>30000000) { b->failed=true; break; }
                if (id!=0x16) {
                    item->server_x=(int32_t)llround(next_x*32); item->server_y=(int32_t)llround(next_y*32); item->server_z=(int32_t)llround(next_z*32);
                    item->entity.x=next_x; item->entity.y=next_y; item->entity.z=next_z;
                }
                item->entity.on_ground=ground!=0;
            }
            break;
        }
        case 0x1c: receive_metadata(c,b); break;
        case 0x21: {
            int x = mc_get_i32(b), z = mc_get_i32(b); bool full = mc_get_u8(b) != 0;
            uint16_t mask = (uint16_t)mc_get_i16(b); int length = mc_get_varint(b);
            if (length < 0 || (size_t)length > b->len - b->pos) b->failed = true;
            else if (!b->failed && !receive_chunk(c, x, z, mask, full, c->dimension == 0, b->data + b->pos, (size_t)length)) b->failed = true;
            break;
        }
        case 0x22: {
            int x = mc_get_i32(b), z = mc_get_i32(b), count = mc_get_varint(b);
            if (x < -1875000 || x > 1875000 || z < -1875000 || z > 1875000 || count < 0 || count > 65536) { b->failed = true; break; }
            for (int i = 0; i < count && !b->failed; ++i) {
                uint16_t pos = (uint16_t)mc_get_i16(b); int state = mc_get_varint(b);
                if (state < 0 || state > 65535) { b->failed = true; break; }
                if (!b->failed) { mc_world_set(&c->world, x * 16 + ((pos >> 12) & 15), pos & 255, z * 16 + ((pos >> 8) & 15), (uint16_t)state); ++c->block_updates; }
            }
            break;
        }
        case 0x23: {
            int x, y, z; mc_get_position(b, &x, &y, &z); int state = mc_get_varint(b);
            if (state < 0 || state > 65535) b->failed = true;
            if (!b->failed) { mc_world_set(&c->world, x, y, z, (uint16_t)state); ++c->block_updates; }
            break;
        }
        case 0x26: {
            bool skylight = mc_get_u8(b) != 0; int count = mc_get_varint(b);
            typedef struct { int x, z; uint16_t mask; } chunk_meta;
            if (count < 0 || count > 4096 || (size_t)count > (b->len - b->pos) / 10u) { b->failed = true; break; }
            chunk_meta *metadata = calloc((size_t)(count ? count : 1), sizeof(*metadata));
            if (!metadata) { b->failed = true; break; }
            for (int i = 0; i < count; ++i) { metadata[i].x = mc_get_i32(b); metadata[i].z = mc_get_i32(b); metadata[i].mask = (uint16_t)mc_get_i16(b); }
            for (int i = 0; i < count && !b->failed; ++i) {
                size_t length = chunk_size(metadata[i].mask, skylight, true);
                if (length > b->len - b->pos || !receive_chunk(c, metadata[i].x, metadata[i].z, metadata[i].mask, true, skylight, b->data + b->pos, length)) b->failed = true;
                else b->pos += length;
            }
            free(metadata);
            break;
        }
        case 0x2b: {
            unsigned reason=mc_get_u8(b); float value=mc_get_f32(b);
            if (!isfinite(value) || b->pos!=b->len) b->failed=true;
            else if (reason==3) {
                if (value<0 || value>3 || value!=(float)(int)value) b->failed=true;
                else {
                    c->gamemode=(int)value;
                    c->can_fly=c->gamemode==1 || c->gamemode==3;
                    if (!c->can_fly) { c->flying=false; c->velocity_y=0; }
                    else if (c->gamemode==3) c->flying=true;
                    if (c->gamemode!=1) {
                        c->creative_open=false;
                        snprintf(c->inventory_status,sizeof(c->inventory_status),"Server changed game mode; creative selection is unavailable");
                    }
                }
            }
            break;
        }
        case 0x38: handle_player_info(c, b); break;
        case 0x2d: receive_open_window(c,b); break;
        case 0x2e: {
            (void)mc_get_u8(b);
            if (b->failed || b->pos!=b->len) { b->failed=true; break; }
            cancel_inventory_actions(c);
            close_inventory(c,false);
            snprintf(c->inventory_status,sizeof(c->inventory_status),"Server closed inventory");
            break;
        }
        case 0x2f: receive_slot(c,b); break;
        case 0x30: receive_inventory(c,b); break;
        case 0x32: receive_transaction(c,b); break;
        case 0x34: if (!mc_maps_receive(&c->maps,b)) b->failed=true; break;
        case 0x39: {
            int flags = mc_get_u8(b); (void)mc_get_f32(b); (void)mc_get_f32(b);
            c->can_fly = (flags & 4) != 0; c->flying = (flags & 2) != 0;
            break;
        }
        case 0x40: {
            char reason[256];
            if (read_chat(b, reason, sizeof(reason))) { snprintf(c->status, sizeof(c->status), "Disconnected: %.235s", reason); c->disconnected = true; fprintf(stderr, "%s\n", c->status); }
            break;
        }
        case 0x48: {
            skip_string(b); char hash[41]; mc_get_string(b, hash, sizeof(hash));
            if (!b->failed) { start_packet(&reply, 0x19); mc_put_string(&reply, hash); mc_put_varint(&reply, 1); send_packet(c, &reply); }
            break;
        }
        default: break; /* Independent packet frames permit safely ignoring unsupported services. */
    }
}

static void poll_network(mc_client *c) {
    if (!mc_conn_poll(&c->connection)) { client_error(c, c->connection.error[0] ? c->connection.error : "Connection closed by server"); return; }
    for (int i = 0; i < 256 && !c->failed && !c->disconnected; ++i) {
        mc_buf packet;
        mc_buf_init(&packet);
        int result = mc_conn_next(&c->connection, &packet);
        if (result < 0) client_error(c, c->connection.error);
        if (result <= 0) { mc_buf_free(&packet); break; }
        c->last_receive_ms = mc_time_ms(); ++c->packets_received;
        handle_packet(c, &packet);
        if (packet.failed) client_error(c, "Malformed or unsupported protocol 47 packet data");
        mc_buf_free(&packet);
    }
    if (c->joined && c->positioned && c->world.count && !c->failed && !c->disconnected) snprintf(c->status, sizeof(c->status), "Connected");
    if (mc_time_ms() - c->last_receive_ms > 30000) client_error(c, "Timed out waiting for server data");
}

static bool body_loaded(const mc_client *c, double x, double z) {
    int first_x = mc_floor_div16((int)floor(x - 0.299)), last_x = mc_floor_div16((int)floor(x + 0.299));
    int first_z = mc_floor_div16((int)floor(z - 0.299)), last_z = mc_floor_div16((int)floor(z + 0.299));
    for (int cz = first_z; cz <= last_z; ++cz) for (int cx = first_x; cx <= last_x; ++cx) {
        bool found = false;
        for (int i = 0; i < c->world.count; ++i)
            if (c->world.chunks[i].x == cx && c->world.chunks[i].z == cz) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

static bool collides(const mc_client *c, double x, double y, double z) {
    if (!body_loaded(c, x, z)) return true;
    if (y < -1000000 || y + 1.8 > 1000000) return true;
    for (int by = (int)floor(y + 0.001)-1; by <= (int)floor(y + 1.799); ++by)
        for (int bz = (int)floor(z - 0.299); bz <= (int)floor(z + 0.299); ++bz)
            for (int bx = (int)floor(x - 0.299); bx <= (int)floor(x + 0.299); ++bx)
            {
                mc_box boxes[3]; unsigned count=mc_block_collision(mc_world_get(&c->world,bx,by,bz),boxes);
                for (unsigned i=0;i<count;i++) {
                    const mc_box *box=&boxes[i];
                    if (x+0.299>bx+box->min_x && x-0.299<bx+box->max_x &&
                        y+1.799>by+box->min_y && y+0.001<by+box->max_y &&
                        z+0.299>bz+box->min_z && z-0.299<bz+box->max_z) return true;
                }
            }
    return false;
}

static void move_axis(mc_client *c, double distance, int axis) {
    int steps = (int)ceil(fabs(distance) / 0.15);
    if (!steps) return;
    double step = distance / steps;
    for (int i = 0; i < steps; ++i) {
        double x = c->x + (axis == 0 ? step : 0), y = c->y + (axis == 1 ? step : 0), z = c->z + (axis == 2 ? step : 0);
        if (collides(c, x, y, z)) {
            if (axis == 1) { c->on_ground = step < 0; c->velocity_y = 0; }
            return;
        }
        c->x = x; c->y = y; c->z = z;
    }
}

bool mc_client_ray(const mc_client *c, int *x, int *y, int *z, int *face) {
    double yaw = c->yaw * 0.0174532925199433, pitch = c->pitch * 0.0174532925199433;
    double dx = -sin(yaw) * cos(pitch), dy = -sin(pitch), dz = cos(yaw) * cos(pitch);
    int previous_x = (int)floor(c->x), previous_y = (int)floor(c->y + 1.62), previous_z = (int)floor(c->z);
    for (double distance = 0; distance <= 6; distance += 0.025) {
        int bx = (int)floor(c->x + dx * distance), by = (int)floor(c->y + 1.62 + dy * distance), bz = (int)floor(c->z + dz * distance);
        if (mc_world_get(&c->world, bx, by, bz) >> 4) {
            *x = bx; *y = by; *z = bz;
            *face = bx > previous_x ? 4 : bx < previous_x ? 5 : by > previous_y ? 0 : by < previous_y ? 1 : bz > previous_z ? 2 : 3;
            return true;
        }
        previous_x = bx; previous_y = by; previous_z = bz;
    }
    return false;
}

static bool send_placement(mc_client *c,C08ValueBlockPos position,int face,const mc_slot *held,bool air) {
    C08PacketPlayerBlockPlacementValue placement; C08PacketPlayerBlockPlacementValue_init(&placement);
    const mc_slot *stack=held->item_id<0 ? NULL : held;
    bool ready=air ? C08PacketPlayerBlockPlacementValue_constructUseItem(&placement,stack) :
        C08PacketPlayerBlockPlacementValue_construct(&placement,&position,face,stack,0.5f,0.5f,0.5f);
    mc_buf packet; start_packet(&packet,0x08);
    if (!ready || !C08PacketPlayerBlockPlacementValue_writePacketData(&placement,&packet)) packet.failed=true;
    C08PacketPlayerBlockPlacementValue_free(&placement);
    return send_packet(c,&packet);
}
static void edit_block(mc_client *c, bool place) {
    if (c->gamemode==3 || (!place && c->gamemode!=1)) return;
    int x=0,y=0,z=0,face=0; bool hit=mc_client_ray(c,&x,&y,&z,&face);
    const mc_slot *held=&c->inventory.slots[MC_HOTBAR_START+c->selected];
    bool use_map=place && held->item_id==395 && (!hit || (mc_world_get(&c->world,x,y,z)>>4)!=58);
    if (!hit && !use_map) return;
    mc_buf packet;
    if (hit) {
        if (place) { if (!send_placement(c,(C08ValueBlockPos){x,y,z},face,held,false)) return; }
        else {
            start_packet(&packet,0x07); mc_put_varint(&packet,0);
            mc_put_position(&packet,x,y,z); mc_put_u8(&packet,(uint8_t)face);
            if (!send_packet(c,&packet)) return;
        }
    }
    /* A non-activating block use falls through to the held empty map's air
       use, matching the same right-click path when no block is in reach. */
    if (use_map && !send_placement(c,(C08ValueBlockPos){-1,-1,-1},255,held,true)) return;
    start_packet(&packet,0x0a); send_packet(c,&packet);
}

static void update_player(mc_client *c, const mc_input *input, double dt) {
    c->paused = input->paused; c->chat_open = input->chat_open;
    if (!c->joined || !c->positioned || c->failed || c->disconnected) return;
    bool controls_active=!input->paused && !input->chat_open;
    bool inventory_controls=controls_active && (!input->inventory_generation_set || input->inventory_generation==c->window_generation);
    if (input->toggle_inventory && inventory_controls) {
        if (c->inventory_open) close_inventory(c,true);
        else {
            c->inventory_open=true; c->creative_open=false;
            snprintf(c->inventory_status,sizeof(c->inventory_status),"Left/right click, Shift transfer, 1-9 swap; Q drop, drag to distribute");
            mc_buf packet; start_packet(&packet,0x16); mc_put_varint(&packet,2); send_packet(c,&packet);
        }
    }
    if (input->toggle_creative && inventory_controls && c->inventory_open && !c->window_id && c->gamemode==1) c->creative_open=!c->creative_open;
    if (input->inventory_click && inventory_controls && c->inventory_open && c->inventory_queue_index==c->inventory_queue_count)
        inventory_click(c,input->inventory_slot,input->inventory_button,input->inventory_mode);
    if (input->inventory_drag && inventory_controls && c->inventory_open && !c->inventory_pending && !c->inventory_sync &&
        c->inventory_queue_index==c->inventory_queue_count && input->inventory_drag_mode<=2 &&
        mc_client_inventory_ready(c) && !(input->inventory_drag_slots>>mc_client_window_slots(c)) && !(input->inventory_drag_slots&1) &&
        (input->inventory_drag_mode<2 || c->gamemode==1)) {
        unsigned base=input->inventory_drag_mode*4; c->inventory_queue_count=0; c->inventory_queue_index=0;
        int *action=c->inventory_queue[c->inventory_queue_count++]; action[0]=-999; action[1]=(int)base; action[2]=5;
        for (unsigned i=1;i<mc_client_window_slots(c);i++) if (input->inventory_drag_slots&(UINT64_C(1)<<i)) {
            action=c->inventory_queue[c->inventory_queue_count++]; action[0]=(int)i; action[1]=(int)base+1; action[2]=5;
        }
        action=c->inventory_queue[c->inventory_queue_count++]; action[0]=-999; action[1]=(int)base+2; action[2]=5;
    }
    if (controls_active && c->inventory_open && c->inventory_queue_index<c->inventory_queue_count && !c->inventory_pending && !c->inventory_sync) {
        int *action=c->inventory_queue[c->inventory_queue_index++]; inventory_click(c,action[0],action[1],action[2]);
        if (!c->inventory_pending) { c->inventory_queue_count=0; c->inventory_queue_index=0; }
    }
    if (c->inventory_close_requested && controls_active && !c->inventory_pending && !c->inventory_sync &&
        c->inventory_queue_index==c->inventory_queue_count) close_inventory(c,true);
    if (input->drop_item && controls_active && !c->inventory_open) drop_held_item(c,input->drop_all);
    if (input->creative_pick>=0 && inventory_controls && c->inventory_open) creative_pick(c,(unsigned)input->creative_pick);
    if (input->chat_submit && input->chat[0]) {
        mc_buf packet; start_packet(&packet, 0x01); mc_put_string(&packet, input->chat); send_packet(c, &packet);
    }
    if (controls_active && input->select_slot >= 0 && input->select_slot < 9) send_slot(c, input->select_slot);
    uint64_t now=mc_time_ms();
    if (!c->last_item_tick_ms) c->last_item_tick_ms=now;
    unsigned ticks=(unsigned)((now-c->last_item_tick_ms)/50); if (ticks>4) ticks=4;
    c->last_item_tick_ms+=ticks*50u;
    for (unsigned step=0;step<ticks;step++) for (unsigned i=0;i<MC_MAX_ITEM_ENTITIES;i++) {
        mc_client_item *item=&c->items[i];
        if (item->active && item->metadata_ready && body_loaded(c,item->entity.x,item->entity.z))
            (void)mc_item_entity_tick(&item->entity,&c->world);
    }
    if (!body_loaded(c, c->x, c->z)) {
        c->velocity_y = 0;
        snprintf(c->status, sizeof(c->status), "Waiting for terrain at player position");
        if (mc_time_ms() - c->last_move_ms >= 50) send_movement(c);
        return;
    }
    if (!input->paused && !input->chat_open && !c->inventory_open) {
        c->yaw += input->look_x * 0.12f; c->pitch += input->look_y * 0.12f;
        c->yaw = fmodf(c->yaw, 360.0f);
        if (c->pitch < -89.5f) c->pitch = -89.5f;
        if (c->pitch > 89.5f) c->pitch = 89.5f;
        if (input->toggle_flight && c->can_fly) { c->flying = !c->flying; c->velocity_y = 0; send_abilities(c); }
        double forward = (input->forward ? 1 : 0) - (input->backward ? 1 : 0);
        double strafe = (input->right ? 1 : 0) - (input->left ? 1 : 0);
        double magnitude = sqrt(forward * forward + strafe * strafe);
        if (magnitude > 1) { forward /= magnitude; strafe /= magnitude; }
        double yaw = c->yaw * 0.0174532925199433;
        double speed = (c->flying ? 6.0 : 4.3) * (input->sprint ? 1.8 : 1.0) * dt;
        move_axis(c, (-sin(yaw) * forward - cos(yaw) * strafe) * speed, 0);
        move_axis(c, (cos(yaw) * forward - sin(yaw) * strafe) * speed, 2);
        if (c->flying) {
            c->on_ground = false;
            move_axis(c, ((input->up ? 1 : 0) - (input->down ? 1 : 0)) * speed, 1);
        } else {
            if (input->up && c->on_ground) { c->velocity_y = 7.0; c->on_ground = false; }
            c->velocity_y -= 20.0 * dt;
            if (c->velocity_y < -45) c->velocity_y = -45;
            c->on_ground = false;
            move_axis(c, c->velocity_y * dt, 1);
        }
        if (input->break_block) edit_block(c, false);
        if (input->place_block) edit_block(c, true);
    }
    if (mc_time_ms() - c->last_move_ms >= 50) send_movement(c);
}

static bool connect_client(mc_client *c) {
    char error[160];
    mc_socket socket = mc_net_connect(c->host, c->port, error, sizeof(error));
    if (socket == MC_INVALID_SOCKET) { client_error(c, error); return false; }
    mc_conn_init(&c->connection, socket);
    c->last_receive_ms = mc_time_ms();
    mc_buf packet;
    start_packet(&packet, 0x00);
    mc_put_varint(&packet, MC_PROTOCOL_VERSION); mc_put_string(&packet, c->host); mc_put_i16(&packet, (int16_t)c->port); mc_put_varint(&packet, 2);
    if (!send_packet(c, &packet)) return false;
    start_packet(&packet, 0x00); mc_put_string(&packet, c->name);
    if (!send_packet(c, &packet)) return false;
    snprintf(c->status, sizeof(c->status), "Logging in to %.180s:%u", c->host, (unsigned)c->port);
    return true;
}

static int self_test(void) {
    mc_client *c = calloc(1, sizeof(*c));
    if (!c) return 1;
    mc_world_init(&c->world, 0); c->state = 1;
    mc_inventory_init(&c->inventory); mc_inventory_init(&c->inventory_authoritative);
    mc_container_init(&c->container,MC_CONTAINER_PLAYER); mc_container_init(&c->container_authoritative,MC_CONTAINER_PLAYER); mc_maps_init(&c->maps);
    size_t length = chunk_size(1, true, true);
    uint8_t *data = calloc(length, 1);
    int errors = 0;
    if (!data) { free(c); return 1; }
#define CHECK(condition, message) do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", message); ++errors; } } while (0)
    data[0] = 0x53; data[1] = 0x12;
    CHECK(receive_chunk(c, -1, 0, 1, true, true, data, length), "full chunk is accepted");
    CHECK(mc_world_get(&c->world, -16, 0, 0) == 0x1253, "chunk block state is little endian at negative coordinate");
    CHECK(!receive_chunk(c, 0, 0, 1, true, true, data, length - 1), "truncated chunk rejected");
    CHECK(!receive_chunk(c, 2000000, 0, 1, true, true, data, length), "out of range chunk coordinate rejected");
    CHECK(receive_chunk(c, -1, 0, 2, false, true, data, chunk_size(2, true, false)), "partial section accepted");
    CHECK(mc_world_get(&c->world, -16, 0, 0) == 0x1253 && mc_world_get(&c->world, -16, 16, 0) == 0x1253, "partial section preserves existing section");
    CHECK(receive_chunk(c, -1, 0, 0, true, true, NULL, 0) && c->world.count == 0, "zero length vanilla unload accepted");
    CHECK(correct_position(c, 4, 70, -3, 720, 20, 0) && c->yaw == 0, "valid position and yaw normalization");
    CHECK(correct_position(c, 2, 1, 5, 15, -10, 31) && c->x == 6 && c->y == 71 && c->z == 2 && c->pitch == 10, "relative correction applied safely");
    CHECK(!correct_position(c, 30000000, 70, 0, 0, 0, 0), "exclusive world position boundary rejected");
    CHECK(!correct_position(c, 0, 70, 0, 0, 0, 128), "unknown correction flags rejected");
    CHECK(!correct_position(c, 0, 70, 0, 0, 91, 0), "invalid pitch rejected");
    c->x = DBL_MAX;
    CHECK(!correct_position(c, DBL_MAX, 0, 0, 0, 0, 1), "relative coordinate overflow rejected before integer casts");
    c->x = 0;
    CHECK(!body_loaded(c, 8.5, 8.5), "unloaded player column blocks physics");
    CHECK(receive_chunk(c, 1, 0, 1, true, true, data, length) && !body_loaded(c, 8.5, 8.5), "remote chunk does not unlock spawn physics");
    CHECK(receive_chunk(c, 0, 0, 1, true, true, data, length) && body_loaded(c, 8.5, 8.5), "spawn chunk unlocks physics");
    CHECK(collides(c, 8.5, 70, -0.1), "unloaded boundary treated as blocked");
    CHECK(!collides(c, 8.5, 300, 8.5), "creative movement above build height remains possible");
    CHECK(mc_world_set(&c->world,8,10,8,(uint16_t)(44u<<4)),"slab fixture placed");
    CHECK(!collides(c,8.5,10.5,8.5) && collides(c,8.5,10.4,8.5),"slab collision respects half height");
    CHECK(mc_world_set(&c->world,8,10,8,(uint16_t)(85u<<4)),"fence fixture placed");
    CHECK(collides(c,8.5,11.3,8.5) && !collides(c,8.5,11.5,8.5),"tall fence collision includes block below player feet");
    mc_buf inv_packet; start_packet(&inv_packet,0x30); mc_put_u8(&inv_packet,0); mc_put_i16(&inv_packet,45);
    mc_slot tool; mc_slot_init(&tool); CHECK(mc_slot_set(&tool,276,1,7),"durable tool slot fixture");
    for (int i=0;i<45;i++) mc_slot_write(&inv_packet,i==36 ? &tool : &c->inventory.slots[i]);
    handle_packet(c,&inv_packet);
    CHECK(!inv_packet.failed && c->inventory_ready && c->inventory.slots[36].item_id==276 && c->inventory.slots[36].damage==7,"window items retain server tool and damage");
    mc_buf_free(&inv_packet); mc_slot_free(&tool);
    mc_slot named_tool; mc_slot_init(&named_tool); CHECK(mc_slot_set(&named_tool,276,1,8),"named tool fixture");
    mc_buf named_data; mc_buf_init(&named_data); mc_put_u8(&named_data,10); mc_put_i16(&named_data,0);
    mc_put_u8(&named_data,10); mc_put_i16(&named_data,7); mc_put_bytes(&named_data,"display",7);
    mc_put_u8(&named_data,8); mc_put_i16(&named_data,4); mc_put_bytes(&named_data,"Name",4); mc_put_i16(&named_data,360);
    for (int i=0;i<30;i++) mc_put_bytes(&named_data,"\xe6\x97\xa5\xe6\x9c\xac\xe3\x81\xae\xe5\x89\xa3",12);
    mc_put_u8(&named_data,0); mc_put_u8(&named_data,0);
    CHECK(mc_nbt_read(&named_data,&named_tool.nbt),"long Japanese custom name parses");
    char short_name[32]; mc_client_slot_name(&named_tool,short_name,sizeof(short_name));
    CHECK(strlen(short_name)==30 && !memcmp(short_name,"\xe6\x97\xa5",3),"custom name truncates only at UTF8 boundaries");
    mc_slot_free(&named_tool); mc_buf_free(&named_data);
    c->joined=true; c->inventory_open=true; c->last_move_ms=mc_time_ms();
    mc_input inactive; memset(&inactive,0,sizeof(inactive)); inactive.select_slot=-1; inactive.creative_pick=-1;
    inactive.paused=true; inactive.inventory_click=true; inactive.inventory_slot=36;
    update_player(c,&inactive,0);
    CHECK(c->inventory.cursor.item_id<0 && c->inventory.slots[36].item_id==276 && !c->inventory_pending,"paused inventory input cannot move a carried item");
    for (unsigned gui=0;gui<3;gui++) for (unsigned window=0;window<3;window++) {
        CHECK(mc_slot_set(&c->inventory.slots[1],41,1,0) && mc_slot_set(&c->inventory.slots[0],266,9,0) &&
            mc_slot_copy(&c->inventory.cursor,&c->inventory.slots[36]) &&
            mc_inventory_copy(&c->inventory_authoritative,&c->inventory),"forced close owns cursor and crafting fixture");
        c->inventory_open=gui!=0; c->creative_open=gui==2;
        c->inventory_pending=true; c->inventory_sync=true; c->inventory_sync_slots=true; c->inventory_sync_cursor=true;
        c->inventory_close_requested=true; c->inventory_queue_count=3; c->inventory_queue_index=1;
        c->inventory.drag_active=true; c->inventory.drag_slots=UINT64_C(1)<<9;
        c->inventory_authoritative.drag_active=true; c->inventory_authoritative.drag_slots=UINT64_C(1)<<9;
        mc_buf forced; start_packet(&forced,0x2e); mc_put_u8(&forced,(uint8_t)(window==0 ? 0 : window==1 ? 7 : 255));
        handle_packet(c,&forced);
        CHECK(!forced.failed && !c->inventory_open && !c->creative_open && !c->inventory_pending && !c->inventory_sync &&
            !c->inventory_sync_slots && !c->inventory_sync_cursor && !c->inventory_close_requested &&
            !c->inventory_queue_count && !c->inventory_queue_index,"server close cancels pending resynchronization and queued input for every window ID");
        CHECK(c->inventory.cursor.item_id<0 && c->inventory_authoritative.cursor.item_id<0 &&
            !c->inventory.drag_active && !c->inventory.drag_slots && !c->inventory_authoritative.drag_active &&
            !c->inventory_authoritative.drag_slots && c->connection.tx.len==0,"server close clears shared cursor and drag without sending a reply");
        CHECK(c->inventory.slots[1].item_id==(gui==1 ? -1 : 41) && c->inventory.slots[0].item_id==(gui==1 ? -1 : 266) &&
            mc_slot_equal(&c->inventory.slots[1],&c->inventory_authoritative.slots[1]) &&
            mc_slot_equal(&c->inventory.slots[0],&c->inventory_authoritative.slots[0]),"only an open player inventory GUI closes its local crafting grid and output");
        mc_buf_free(&forced);
    }
    mc_buf packet; start_packet(&packet, 0x38); mc_put_varint(&packet, 0); mc_put_varint(&packet, 1);
    uint8_t uuid[16] = {1, 2, 3}; mc_put_bytes(&packet, uuid, 16); mc_put_string(&packet, "VisiblePlayer");
    mc_put_varint(&packet, 0); mc_put_varint(&packet, 1); mc_put_varint(&packet, 0); mc_put_u8(&packet, 0);
    handle_packet(c, &packet); CHECK(!packet.failed, "player info decoded"); mc_buf_free(&packet);
    start_packet(&packet, 0x0c); mc_put_varint(&packet, 42); mc_put_bytes(&packet, uuid, 16);
    mc_put_i32(&packet, 32); mc_put_i32(&packet, 2048); mc_put_i32(&packet, -32); mc_put_u8(&packet, 0); mc_put_u8(&packet, 0); mc_put_i16(&packet, 0); mc_put_u8(&packet, 127);
    handle_packet(c, &packet); mc_remote_player *player = remote_player(c, 42, false);
    CHECK(!packet.failed && player && strcmp(player->name, "VisiblePlayer") == 0 && player->z == -1, "info before spawn resolves UUID and position"); mc_buf_free(&packet);
    char text[256]; plain_chat("{\"text\":\"Hello \",\"extra\":[{\"text\":\"world\"}]}", text, sizeof(text));
    CHECK(strcmp(text, "Hello world") == 0, "chat extras retain text");
    plain_chat("{\"text\":\"\\u65e5\\u672c \\ud83d\\ude00\"}", text, sizeof(text));
    CHECK(strcmp(text, "\xe6\x97\xa5\xe6\x9c\xac \xf0\x9f\x98\x80") == 0, "Japanese and paired surrogate chat decoded as UTF8");
    free(data); mc_inventory_free(&c->inventory); mc_inventory_free(&c->inventory_authoritative); mc_container_free(&c->container); mc_container_free(&c->container_authoritative); mc_maps_free(&c->maps); mc_world_free(&c->world); free(c);
    if (!errors) printf("client self-test passed\n");
    return errors ? 1 : 0;
#undef CHECK
}

static void usage(const char *program) {
    printf("Usage: %s [--host HOST] [--port 25565] [--name Player] [--headless]\n"
           "       [--run-seconds SECONDS] [--screenshot FRAME.ppm] [--self-test]\n"
           "       [--inventory-actions SLOT:BUTTON:MODE,...] (headless transaction exercise)\n"
           "       [--drop-actions one,all,...] [--close-inventory] (shared native input paths)\n"
           "       [--use-block X,Y,Z] (aim and right-click through shared native input)\n"
           "Offline protocol 47 (Minecraft 1.8.x). Windows GUI; original procedural materials.\n"
           "WASD move; mouse look; Space jump/ascend; Shift descend; Ctrl sprint; F flight.\n"
           "Left/right mouse remove/place; 1-9 hotbar; E inventory; T chat, Enter send; Esc pause/cursor.\n", program);
}

static bool parse_inventory_actions(const char *input,int actions[64][3],unsigned *count) {
    const char *cursor=input; unsigned parsed=0;
    if (!*cursor) return false;
    do {
        if (parsed==64) return false;
        for (unsigned column=0;column<3;column++) {
            if ((*cursor<'0' || *cursor>'9') && !(column==0 && *cursor=='-')) return false;
            char *end; errno=0; long value=strtol(cursor,&end,10);
            if (errno || end==cursor || (value<0 && !(column==0 && value==-999)) || value>(column==0 ? 45 : column==1 ? 10 : 6)) return false;
            actions[parsed][column]=(int)value; cursor=end;
            if (column<2) { if (*cursor!=':') return false; ++cursor; }
        }
        if (actions[parsed][1]>8 && actions[parsed][2]!=5) return false;
        ++parsed;
        if (!*cursor) break;
        if (*cursor!=',') return false;
        ++cursor; if (!*cursor) return false;
    } while (*cursor);
    *count=parsed; return true;
}
static bool parse_drop_actions(const char *input,bool actions[64],unsigned *count) {
    unsigned parsed=0; const char *cursor=input;
    if (!*cursor) return false;
    while (*cursor) {
        if (parsed==64) return false;
        const char *end=strchr(cursor,','); size_t size=end ? (size_t)(end-cursor) : strlen(cursor);
        if (size==3 && !memcmp(cursor,"one",3)) actions[parsed++]=false;
        else if (size==3 && !memcmp(cursor,"all",3)) actions[parsed++]=true;
        else return false;
        if (!end) break;
        cursor=end+1; if (!*cursor) return false;
    }
    *count=parsed; return true;
}
static bool parse_use_target(const char *text,int target[3]) {
    const char *p=text;
    for (unsigned i=0;i<3;i++) {
        if ((*p<'0' || *p>'9') && *p!='-') return false;
        char *end; errno=0; long value=strtol(p,&end,10);
        if (errno || end==p || (i==1 ? value<0 || value>255 : value<=-30000000 || value>=30000000)) return false;
        target[i]=(int)value; p=end;
        if (i<2) { if (*p++!=',') return false; } else if (*p) return false;
    }
    return true;
}

int main(int argc, char **argv) {
    mc_client *c = calloc(1, sizeof(*c));
    if (!c) { fprintf(stderr, "Out of memory\n"); return 1; }
    snprintf(c->host, sizeof(c->host), "127.0.0.1"); snprintf(c->name, sizeof(c->name), "C919Player");
    c->port = 25565; c->connection.socket = MC_INVALID_SOCKET;
    bool headless = false; double run_seconds = 0; const char *screenshot = NULL;
    int actions[64][3]; unsigned action_count=0,action_index=0;
    bool drops[64],close_script=false,close_done=false; unsigned drop_count=0,drop_index=0;
    bool use_script=false,use_sent=false; int use_target[3]={0};
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) { usage(argv[0]); free(c); return 0; }
        if (strcmp(arg, "--self-test") == 0) { free(c); return self_test(); }
        if (strcmp(arg, "--headless") == 0) { headless = true; continue; }
        if (strcmp(arg,"--close-inventory")==0) { close_script=true; continue; }
        if (i + 1 >= argc) { fprintf(stderr, "Missing value for %s\n", arg); free(c); return 2; }
        const char *value = argv[++i];
        if (strcmp(arg, "--host") == 0 && *value && strlen(value) < sizeof(c->host)) snprintf(c->host, sizeof(c->host), "%s", value);
        else if (strcmp(arg, "--name") == 0 && *value && strlen(value) <= 16) {
            for (const char *p = value; *p; ++p) if (!isalnum((unsigned char)*p) && *p != '_') { fprintf(stderr, "Name must contain 1-16 ASCII letters, numbers, or underscores\n"); free(c); return 2; }
            snprintf(c->name, sizeof(c->name), "%s", value);
        } else if (strcmp(arg, "--port") == 0) {
            char *end; errno = 0; long port = strtol(value, &end, 10);
            if (errno || *end || port < 1 || port > 65535) { fprintf(stderr, "Invalid port\n"); free(c); return 2; } c->port = (uint16_t)port;
        } else if (strcmp(arg, "--run-seconds") == 0) {
            char *end; errno = 0; run_seconds = strtod(value, &end);
            if (errno || *end || !isfinite(run_seconds) || run_seconds <= 0 || run_seconds > 86400) { fprintf(stderr, "Invalid run duration\n"); free(c); return 2; }
        } else if (strcmp(arg, "--screenshot") == 0 && *value) screenshot = value;
        else if (strcmp(arg,"--inventory-actions")==0) {
            if (!parse_inventory_actions(value,actions,&action_count)) { fprintf(stderr,"Invalid inventory actions\n"); free(c); return 2; }
        }
        else if (strcmp(arg,"--drop-actions")==0) {
            if (!parse_drop_actions(value,drops,&drop_count)) { fprintf(stderr,"Invalid drop actions\n"); free(c); return 2; }
        }
        else if (strcmp(arg,"--use-block")==0) {
            if (!parse_use_target(value,use_target)) { fprintf(stderr,"Invalid block-use target\n"); free(c); return 2; }
            use_script=true;
        }
        else { fprintf(stderr, "Invalid argument: %s\n", arg); usage(argv[0]); free(c); return 2; }
    }
    if (headless && screenshot) { fprintf(stderr, "--screenshot needs the Windows renderer; omit --headless\n"); free(c); return 2; }
    if ((headless || screenshot) && run_seconds == 0) run_seconds = 5;
    if (!mc_net_init()) { fprintf(stderr, "Network initialization failed\n"); free(c); return 1; }
    mc_world_init(&c->world, 0);
    mc_inventory_init(&c->inventory); mc_inventory_init(&c->inventory_authoritative); c->next_inventory_action=1;
    mc_container_init(&c->container,MC_CONTAINER_PLAYER); mc_container_init(&c->container_authoritative,MC_CONTAINER_PLAYER); mc_maps_init(&c->maps);
    mc_renderer *renderer = NULL;
    if (!headless) {
        char error[160]; renderer = mc_renderer_open(screenshot != NULL, error, sizeof(error));
        if (!renderer) { fprintf(stderr, "Renderer: %s\n", error); mc_net_shutdown(); free(c); return 1; }
    }
    connect_client(c);
    uint64_t started = mc_time_ms(), previous = started;
    bool screenshot_done = false;
    while (true) {
        uint64_t now = mc_time_ms(); double dt = (now - previous) / 1000.0; previous = now;
        if (dt > 0.05) dt = 0.05;
        mc_input input; memset(&input, 0, sizeof(input)); input.select_slot = -1; input.creative_pick=-1;
        if (renderer) mc_renderer_poll(renderer, &input);
        if (input.quit) break;
        if (!c->failed && !c->disconnected) {
            poll_network(c);
            if (use_script && !use_sent && c->joined && c->positioned && mc_client_inventory_ready(c) &&
                body_loaded(c,c->x,c->z) && body_loaded(c,use_target[0]+0.5,use_target[2]+0.5) && !c->inventory_open) {
                double dx=use_target[0]+0.5-c->x,dy=use_target[1]+0.5-c->y-1.62,dz=use_target[2]+0.5-c->z;
                c->yaw=(float)(atan2(-dx,dz)*57.29577951308232); c->pitch=(float)(-atan2(dy,hypot(dx,dz))*57.29577951308232);
                input.place_block=true; use_sent=true;
            }
            bool script_ready=mc_client_inventory_ready(c) && (!use_script || (use_sent && c->window_id));
            if (action_index<action_count && script_ready && !c->inventory_pending && !c->inventory_sync) {
                c->inventory_open=true; input.inventory_click=true;
                input.inventory_slot=actions[action_index][0]; input.inventory_button=actions[action_index][1]; input.inventory_mode=actions[action_index][2]; ++action_index;
            } else if (action_index==action_count && close_script && !close_done && script_ready && !c->inventory_pending && !c->inventory_sync) {
                c->inventory_open=true; input.toggle_inventory=true; close_done=true;
            } else if (drop_index<drop_count && c->inventory_ready && !c->inventory_pending && !c->inventory_sync && !c->inventory_open) {
                input.drop_item=true; input.drop_all=drops[drop_index++];
            }
            update_player(c, &input, dt);
        }
        if (renderer) mc_renderer_draw(renderer, c);
        if (screenshot && !screenshot_done && c->joined && c->positioned && c->world.count && now - started >= 500) {
            char error[160]; screenshot_done = mc_renderer_screenshot(renderer, screenshot, error, sizeof(error));
            if (!screenshot_done) client_error(c, error);
            else printf("SCREENSHOT %s\n", screenshot);
        }
        if (headless && (c->failed || c->disconnected)) break;
        if (run_seconds > 0 && now - started >= (uint64_t)(run_seconds * 1000)) break;
        mc_sleep_ms(renderer ? 8 : 5);
    }
    unsigned non_air = 0, players = 0,items=0;
    for (int i = 0; i < c->world.count; ++i) for (unsigned b = 0; b < MC_CHUNK_BLOCKS; ++b) if (c->world.chunks[i].blocks[b] >> 4) ++non_air;
    for (int i = 0; i < MC_CLIENT_PLAYERS; ++i) if (c->players[i].active) ++players;
    for (unsigned i=0;i<MC_MAX_ITEM_ENTITIES;i++) if (c->items[i].active) ++items;
    printf("CLIENT_RESULT joined=%d positioned=%d chunks=%d chunk_packets=%u non_air=%u block_updates=%u players=%u packets=%u position=%.3f,%.3f,%.3f inventory_packets=%u inventory_rejections=%u inventory_pending=%d gamemode=%d item_entities=%u item_spawns=%u item_metadata=%u item_collects=%u\n",
           c->joined, c->positioned, c->world.count, c->chunks_received, non_air, c->block_updates, players, c->packets_received, c->x, c->y, c->z,c->inventory_packets,c->inventory_rejections,c->inventory_pending,c->gamemode,items,c->item_spawns,c->item_metadata,c->item_collects);
    for (unsigned i=0;i<MC_MAX_ITEM_ENTITIES;i++) if (c->items[i].active) {
        const mc_client_item *item=&c->items[i]; const mc_slot *s=&item->entity.item;
        printf("CLIENT_ITEM eid=%d ready=%d id=%d count=%u damage=%d nbt_size=%zu nbt_crc=%08lx server_position=%.3f,%.3f,%.3f position=%.3f,%.3f,%.3f\n",
            item->entity.eid,item->metadata_ready,s->item_id,(unsigned)s->count,s->damage,s->nbt.size,(unsigned long)crc32(0,s->nbt.data,(uInt)s->nbt.size),
            item->server_x/32.0,item->server_y/32.0,item->server_z/32.0,item->entity.x,item->entity.y,item->entity.z);
    }
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) {
        const mc_slot *slot=&c->inventory.slots[i];
        if (slot->item_id>=0) printf("CLIENT_SLOT index=%d id=%d count=%u damage=%d nbt_size=%zu nbt_crc=%08lx\n",i,slot->item_id,(unsigned)slot->count,slot->damage,slot->nbt.size,(unsigned long)crc32(0,slot->nbt.data,(uInt)slot->nbt.size));
    }
    printf("CLIENT_WINDOW id=%u ready=%d slots=%u generation=%llu sync=%d title=%s\n",(unsigned)c->window_id,
        c->window_id ? c->window_ready : c->inventory_ready,mc_client_window_slots(c),(unsigned long long)c->window_generation,c->inventory_sync,c->window_title);
    for (int i=0;c->window_id && i<10;i++) {
        const mc_slot *s=&c->container.slots[i];
        if (s->item_id>=0) printf("CLIENT_CONTAINER_SLOT index=%d id=%d count=%u damage=%d nbt_size=%zu nbt_crc=%08lx\n",i,s->item_id,
            (unsigned)s->count,s->damage,s->nbt.size,(unsigned long)crc32(0,s->nbt.data,(uInt)s->nbt.size));
    }
    for (size_t i=0;i<c->maps.count;i++) {
        const mc_map_info *map=&c->maps.entries[i];
        printf("CLIENT_MAP id=%d scale=%u icons=%zu metadata_known=%d pixels_crc=%08lx\n",map->id,
            (unsigned)map->scale,map->icon_count,map->metadata_known,(unsigned long)crc32(0,map->colors,MC_MAP_PIXELS));
    }
    printf("CLIENT_CURSOR id=%d count=%u damage=%d nbt_size=%zu nbt_crc=%08lx\n",c->inventory.cursor.item_id,(unsigned)c->inventory.cursor.count,c->inventory.cursor.damage,c->inventory.cursor.nbt.size,(unsigned long)crc32(0,c->inventory.cursor.nbt.data,(uInt)c->inventory.cursor.nbt.size));
    int result = c->failed || c->disconnected || ((headless || screenshot) && (!c->joined || !c->positioned || !c->world.count)) || (screenshot && !screenshot_done) ? 1 : 0;
    if (renderer) mc_renderer_close(renderer);
    clear_items(c); mc_conn_close(&c->connection); mc_inventory_free(&c->inventory); mc_inventory_free(&c->inventory_authoritative); mc_container_free(&c->container); mc_container_free(&c->container_authoritative); mc_maps_free(&c->maps); mc_world_free(&c->world); mc_net_shutdown(); free(c);
    return result;
}
