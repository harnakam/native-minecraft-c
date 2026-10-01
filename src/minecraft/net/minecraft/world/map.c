#include "map.h"
#include "item/ItemMap.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool compound(const mc_nbt *n) {
    return n && (n->size ? n->data && n->data[0]==10 && mc_nbt_validate(n->data,n->size) : !n->data);
}
void mc_map_info_init(mc_map_info *m) { if (m) memset(m,0,sizeof(*m)); }
void mc_map_info_free(mc_map_info *m) { if (m) { mc_nbt_free(&m->original_nbt); mc_nbt_free(&m->original_entry_nbt); mc_MapData_free_tracking(m); mc_map_info_init(m); } }
bool mc_map_info_valid(const mc_map_info *m) {
    if (!m || m->scale>4 || m->icon_count>MC_MAX_MAP_ICONS || !compound(&m->original_nbt) || !compound(&m->original_entry_nbt)) return false;
    for (size_t i=0;i<m->icon_count;i++) if (m->icons[i].type>15 || m->icons[i].direction>15) return false;
    return true;
}
bool mc_map_info_copy(mc_map_info *to,const mc_map_info *from) {
    if (!to || !mc_map_info_valid(from)) return false;
    if (to==from) return true;
    mc_map_info copy=*from; copy.tracking=NULL; mc_nbt_init(&copy.original_nbt); mc_nbt_init(&copy.original_entry_nbt);
    if (!mc_nbt_copy(&copy.original_nbt,&from->original_nbt) || !mc_nbt_copy(&copy.original_entry_nbt,&from->original_entry_nbt) || !mc_MapData_copy_tracking(&copy,from)) { mc_map_info_free(&copy); return false; }
    mc_map_info_free(to); *to=copy; return true;
}
void mc_maps_init(mc_maps *m) { if (m) memset(m,0,sizeof(*m)); }
void mc_maps_free(mc_maps *m) {
    if (!m) return;
    for (size_t i=0;i<m->count;i++) mc_map_info_free(&m->entries[i]);
    free(m->entries); mc_nbt_free(&m->original_nbt); mc_maps_init(m);
}
static bool shape(const mc_maps *m) {
    return m && m->count<=m->capacity && m->capacity<=MC_MAX_MAPS &&
        (!m->capacity ? !m->entries : m->entries!=NULL) && m->next_id>=0 && m->next_id<=UINT16_MAX && compound(&m->original_nbt);
}
mc_map_info *mc_maps_find(mc_maps *m,int32_t id) {
    if (!shape(m)) return NULL;
    for (size_t i=0;i<m->count;i++) if (m->entries[i].id==id) return &m->entries[i];
    return NULL;
}
const mc_map_info *mc_maps_find_const(const mc_maps *m,int32_t id) {
    if (!shape(m)) return NULL;
    for (size_t i=0;i<m->count;i++) if (m->entries[i].id==id) return &m->entries[i];
    return NULL;
}
static bool store_valid(const mc_maps *m) {
    if (!shape(m)) return false;
    for (size_t i=0;i<m->count;i++) {
        if (!mc_map_info_valid(&m->entries[i])) return false;
        for (size_t j=0;j<i;j++) if (m->entries[j].id==m->entries[i].id) return false;
    }
    return true;
}
bool mc_maps_copy(mc_maps *to,const mc_maps *from) {
    if (!to || !store_valid(from)) return false;
    if (to==from) return true;
    mc_maps copy={0}; copy.next_id=from->next_id;
    if (!mc_nbt_copy(&copy.original_nbt,&from->original_nbt)) return false;
    if (from->count) {
        copy.entries=calloc(from->count,sizeof(*copy.entries));
        if (!copy.entries) { mc_maps_free(&copy); return false; }
        copy.capacity=from->count;
        for (;copy.count<from->count;copy.count++) {
            if (!mc_map_info_copy(&copy.entries[copy.count],&from->entries[copy.count])) { mc_maps_free(&copy); return false; }
        }
    }
    mc_maps_free(to); *to=copy; return true;
}
static bool add(mc_maps *m,const mc_map_info *map) {
    if (!shape(m) || !mc_map_info_valid(map) || m->count==MC_MAX_MAPS || mc_maps_find(m,map->id)) return false;
    mc_map_info copy={0}; if (!mc_map_info_copy(&copy,map)) return false;
    if (m->count==m->capacity) {
        size_t capacity=m->capacity ? m->capacity*2 : 4;
        if (capacity>MC_MAX_MAPS) capacity=MC_MAX_MAPS;
        mc_map_info *entries=realloc(m->entries,capacity*sizeof(*entries));
        if (!entries) { mc_map_info_free(&copy); return false; }
        m->entries=entries; m->capacity=capacity;
    }
    m->entries[m->count++]=copy; return true;
}
bool mc_maps_add(mc_maps *m,const mc_map_info *map) {
    if (!store_valid(m) || !mc_map_info_valid(map)) return false;
    mc_maps copy={0}; if (!mc_maps_copy(&copy,m)) return false;
    if (!add(&copy,map)) { mc_maps_free(&copy); return false; }
    if (map->metadata_known && map->id>=copy.next_id && map->id<=INT16_MAX) copy.next_id=map->id+1;
    bool all_known=true;
    for (size_t i=0;i<copy.count;i++) if (!copy.entries[i].metadata_known) all_known=false;
    if (all_known) {
        mc_nbt n={0}; bool okay=mc_maps_encode(&copy,&n); mc_nbt_free(&n);
        if (!okay) { mc_maps_free(&copy); return false; }
    }
    mc_maps_free(m); *m=copy; return true;
}
bool mc_map_center(double x,double z,uint8_t scale,int32_t *cx,int32_t *cz) {
    if (!cx || !cz || cx==cz || !isfinite(x) || !isfinite(z) || scale>4) return false;
    double size=(double)(128u<<scale);
    double ax=floor((x+64.0)/size)*size+size/2.0-64.0;
    double az=floor((z+64.0)/size)*size+size/2.0-64.0;
    if (!isfinite(ax) || !isfinite(az) || ax<INT32_MIN || ax>INT32_MAX || az<INT32_MIN || az>INT32_MAX) return false;
    *cx=(int32_t)ax; *cz=(int32_t)az; return true;
}
static bool filled_valid(const mc_slot *s) {
    return s && s->item_id==358 && s->count>=1 && s->count<=127 && s->damage>=0 && compound(&s->nbt);
}
static bool allocate(mc_maps *maps,double x,double z,int dimension,uint8_t scale,int16_t *id) {
    if (!store_valid(maps) || maps->next_id>INT16_MAX || dimension<INT8_MIN || dimension>INT8_MAX) return false;
    int32_t chosen=maps->next_id;
    while (chosen<=INT16_MAX && mc_maps_find(maps,chosen)) chosen++;
    if (chosen>INT16_MAX) return false;
    mc_map_info map={0}; map.id=chosen; map.dimension=(int8_t)dimension; map.scale=scale; map.metadata_known=true;
    if (!mc_map_center(x,z,scale,&map.center_x,&map.center_z) || !add(maps,&map)) return false;
    maps->next_id=chosen+1; *id=(int16_t)chosen; return true;
}
static bool encodable(const mc_maps *maps) {
    mc_nbt n={0}; bool okay=mc_maps_encode(maps,&n); mc_nbt_free(&n); return okay;
}
static void adopt(mc_maps *to,mc_maps *copy,mc_slot *out,mc_slot *item) {
    mc_maps_free(to); *to=*copy; mc_maps_init(copy);
    mc_slot_free(out); *out=*item; mc_slot_init(item);
}
bool mc_maps_create(mc_maps *maps,mc_slot *out,double x,double z,int dimension,uint8_t scale) {
    if (!out || !store_valid(maps)) return false;
    mc_maps copy={0}; mc_slot item; mc_slot_init(&item); int16_t id;
    bool okay=mc_maps_copy(&copy,maps) && allocate(&copy,x,z,dimension,scale,&id) && mc_slot_set(&item,358,1,id) && encodable(&copy);
    if (okay) adopt(maps,&copy,out,&item);
    mc_slot_free(&item); mc_maps_free(&copy); return okay;
}
static bool resolve(mc_maps *maps,mc_slot *item,int32_t x,int32_t z,int dimension) {
    const mc_map_info *known=mc_maps_find_const(maps,item->damage);
    if (known) return known->metadata_known;
    int16_t id; if (!allocate(maps,x,z,dimension,3,&id)) return false;
    item->damage=id; return true;
}
bool mc_maps_resolve(mc_maps *maps,mc_slot *item,int32_t x,int32_t z,int dimension) {
    if (!store_valid(maps) || !filled_valid(item)) return false;
    const mc_map_info *known=mc_maps_find_const(maps,item->damage);
    if (known) return known->metadata_known;
    mc_maps copy={0}; mc_slot result; mc_slot_init(&result);
    bool okay=mc_maps_copy(&copy,maps) && mc_slot_copy(&result,item) && resolve(&copy,&result,x,z,dimension) && encodable(&copy);
    if (okay) adopt(maps,&copy,item,&result);
    mc_slot_free(&result); mc_maps_free(&copy); return okay;
}
static void named(mc_buf *b,uint8_t type,const char *name) {
    size_t len=strlen(name); mc_put_u8(b,type); mc_put_i16(b,(int16_t)len); mc_put_bytes(b,name,len);
}
static bool raw_name(const mc_nbt *n,const char *name) {
    size_t len=strlen(name);
    return n->size>=3+len && (((size_t)n->data[1]<<8)|n->data[2])==len && !memcmp(n->data+3,name,len);
}
static bool unknown(mc_buf *out,const mc_nbt *prior,const char *const *known,size_t count) {
    if (!prior->size) return true;
    mc_nbt_view root; if (!mc_nbt_root(prior,&root) || root.type!=10) return false;
    mc_buf in={0}; in.data=(uint8_t *)root.data; in.len=root.size;
    while (in.pos<in.len && in.data[in.pos]) {
        mc_nbt child={0}; if (!mc_nbt_read(&in,&child)) return false;
        bool skip=false; for (size_t i=0;i<count;i++) if (raw_name(&child,known[i])) skip=true;
        if (!skip) mc_put_bytes(out,child.data,child.size);
        mc_nbt_free(&child); if (out->failed) return false;
    }
    return in.pos+1==in.len && in.data[in.pos]==0;
}
static void root_header(mc_buf *out,const mc_nbt *prior) {
    if (prior->size) mc_put_bytes(out,prior->data,3+(((size_t)prior->data[1]<<8)|prior->data[2]));
    else { mc_put_u8(out,10); mc_put_i16(out,0); }
}
bool mc_maps_scale_preview(const mc_maps *maps,const mc_slot *source,mc_slot *out) {
    if (!out || !filled_valid(source)) return false;
    const mc_map_info *m=mc_maps_find_const(maps,source->damage);
    if (!m || !mc_map_info_valid(m) || !m->metadata_known || m->scale>=4) return false;
    mc_slot copy; mc_slot_init(&copy); if (!mc_slot_copy(&copy,source)) return false;
    copy.count=1;
    static const char *const fields[]={"map_is_scaling"}; mc_buf b={0}; root_header(&b,&source->nbt);
    bool okay=unknown(&b,&source->nbt,fields,1);
    named(&b,1,"map_is_scaling"); mc_put_u8(&b,1); mc_put_u8(&b,0);
    okay=okay && !b.failed && mc_nbt_read(&b,&copy.nbt);
    if (okay) { mc_slot_free(out); *out=copy; mc_slot_init(&copy); }
    mc_slot_free(&copy); mc_buf_free(&b); return okay;
}
static bool scaling(const mc_slot *item) {
    mc_nbt_view root,tag; int64_t value; double number;
    if (!item->nbt.size || !mc_nbt_root(&item->nbt,&root) || !mc_nbt_find(&root,"map_is_scaling",&tag)) return false;
    if (mc_nbt_get_integer(&tag,&value)) return (uint8_t)value!=0;
    if (mc_nbt_get_number(&tag,&number)) {
        /* Java's numeric byte getter floors after a saturating int cast.
           Unsigned subtraction also models the negative-infinity wrap. */
        int32_t integer=isnan(number) ? 0 : number>=INT32_MAX ? INT32_MAX :
            number<=INT32_MIN ? INT32_MIN : (int32_t)number;
        uint32_t floored=(uint32_t)integer;
        if (number<(double)integer) floored--;
        return (uint8_t)floored!=0;
    }
    return false;
}
bool mc_maps_on_crafted(mc_maps *maps,mc_slot *item,int32_t x,int32_t z,int dimension) {
    if (!store_valid(maps) || !filled_valid(item)) return false;
    if (!scaling(item)) return true;
    mc_maps copy={0}; mc_slot result; mc_slot_init(&result);
    bool okay=mc_maps_copy(&copy,maps) && mc_slot_copy(&result,item) && resolve(&copy,&result,x,z,dimension);
    if (okay) {
        const mc_map_info *old=mc_maps_find_const(&copy,result.damage); int16_t id;
        int32_t center_x=old->center_x,center_z=old->center_z; int dim=old->dimension;
        uint8_t scale=old->scale<4 ? (uint8_t)(old->scale+1) : 4;
        okay=allocate(&copy,center_x,center_z,dim,scale,&id);
        if (okay) result.damage=id;
    }
    okay=okay && encodable(&copy);
    if (okay) adopt(maps,&copy,item,&result);
    mc_slot_free(&result); mc_maps_free(&copy); return okay;
}
static bool body(mc_buf *b,const mc_map_info *m) {
    static const char *const fields[]={"dimension","xCenter","zCenter","scale","width","height","colors"};
    if (!mc_map_info_valid(m) || !m->metadata_known || !unknown(b,&m->original_nbt,fields,7)) return false;
    named(b,1,"dimension"); mc_put_u8(b,(uint8_t)m->dimension);
    named(b,3,"xCenter"); mc_put_i32(b,m->center_x); named(b,3,"zCenter"); mc_put_i32(b,m->center_z);
    named(b,1,"scale"); mc_put_u8(b,m->scale); named(b,2,"width"); mc_put_i16(b,128);
    named(b,2,"height"); mc_put_i16(b,128); named(b,7,"colors"); mc_put_i32(b,MC_MAP_PIXELS);
    mc_put_bytes(b,m->colors,MC_MAP_PIXELS); mc_put_u8(b,0); return !b->failed;
}
bool mc_map_info_encode(const mc_map_info *m,mc_nbt *out) {
    if (!out || !mc_map_info_valid(m)) return false;
    mc_buf b={0}; root_header(&b,&m->original_nbt);
    bool okay=body(&b,m) && mc_nbt_read(&b,out); mc_buf_free(&b); return okay;
}
static bool integer(const mc_nbt_view *root,const char *name,int64_t *out) {
    mc_nbt_view v; int64_t value=0;
    if (mc_nbt_find(root,name,&v) && !mc_nbt_get_integer(&v,&value)) return false;
    *out=value; return true;
}
bool mc_map_info_decode(int32_t id,const mc_nbt *input,mc_map_info *out) {
    if (!out || !compound(input) || !input->size) return false;
    mc_nbt_view root,v; int64_t dimension,x,z,scale,width,height;
    if (!mc_nbt_root(input,&root) || !integer(&root,"dimension",&dimension) || !integer(&root,"xCenter",&x) ||
        !integer(&root,"zCenter",&z) || !integer(&root,"scale",&scale) || !integer(&root,"width",&width) ||
        !integer(&root,"height",&height) || x<INT32_MIN || x>INT32_MAX || z<INT32_MIN || z>INT32_MAX ||
        width<0 || width>128 || height<0 || height>128) return false;
    mc_map_info copy={0}; copy.id=id; copy.center_x=(int32_t)x; copy.center_z=(int32_t)z;
    uint8_t dim_bits=(uint8_t)dimension,scale_bits=(uint8_t)scale;
    memcpy(&copy.dimension,&dim_bits,1); int8_t signed_scale; memcpy(&signed_scale,&scale_bits,1);
    copy.scale=signed_scale<0 ? 0 : signed_scale>4 ? 4 : (uint8_t)signed_scale; copy.metadata_known=true;
    size_t length=0; const uint8_t *colors=NULL;
    if (mc_nbt_find(&root,"colors",&v)) {
        if (v.type!=7 || v.size<4) return false;
        mc_buf b={0}; b.data=(uint8_t *)v.data; b.len=v.size;
        int32_t n=mc_get_i32(&b); if (n<0 || (size_t)n!=v.size-4) return false;
        length=(size_t)n; colors=v.data+4;
    }
    if (length!=(size_t)width*(size_t)height) return false;
    unsigned ox=(128u-(unsigned)width)/2,oz=(128u-(unsigned)height)/2;
    for (unsigned j=0;j<(unsigned)height;j++) for (unsigned i=0;i<(unsigned)width;i++)
        copy.colors[i+ox+(j+oz)*128]=colors[i+j*(unsigned)width];
    if (!mc_nbt_copy(&copy.original_nbt,input)) return false;
    mc_map_info_free(out); *out=copy; return true;
}
static bool as_nbt(const mc_nbt_view *v,mc_nbt *out) {
    if (v->type!=10) return false;
    mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0); mc_put_bytes(&b,v->data,v->size);
    bool okay=!b.failed && mc_nbt_read(&b,out); mc_buf_free(&b); return okay;
}
bool mc_maps_encode(const mc_maps *m,mc_nbt *out) {
    static const char *const fields[]={"Version","NextId","Maps"};
    static const char *const entry_fields[]={"Id","Data"};
    if (!out || !store_valid(m)) return false;
    mc_buf b={0}; root_header(&b,&m->original_nbt);
    if (!unknown(&b,&m->original_nbt,fields,3)) goto failed;
    named(&b,3,"Version"); mc_put_i32(&b,1); named(&b,3,"NextId"); mc_put_i32(&b,m->next_id);
    named(&b,9,"Maps"); mc_put_u8(&b,10); mc_put_i32(&b,(int32_t)m->count);
    for (size_t i=0;i<m->count;i++) {
        if (!unknown(&b,&m->entries[i].original_entry_nbt,entry_fields,2)) goto failed;
        named(&b,3,"Id"); mc_put_i32(&b,m->entries[i].id); named(&b,10,"Data");
        if (!body(&b,&m->entries[i])) goto failed;
        mc_put_u8(&b,0);
    }
    mc_put_u8(&b,0);
    if (b.failed || !mc_nbt_read(&b,out)) goto failed;
    mc_buf_free(&b); return true;
failed:
    mc_buf_free(&b); return false;
}
static bool next_compound(mc_buf *in,mc_nbt *out) {
    mc_buf b={0}; mc_put_u8(&b,10); mc_put_i16(&b,0);
    while (in->pos<in->len && in->data[in->pos]) {
        mc_nbt child={0}; if (!mc_nbt_read(in,&child)) { mc_buf_free(&b); return false; }
        mc_put_bytes(&b,child.data,child.size); mc_nbt_free(&child);
        if (b.failed) { mc_buf_free(&b); return false; }
    }
    if (in->pos==in->len || mc_get_u8(in)!=0) { mc_buf_free(&b); return false; }
    mc_put_u8(&b,0); bool okay=!b.failed && mc_nbt_read(&b,out); mc_buf_free(&b); return okay;
}
bool mc_maps_decode(const mc_nbt *input,mc_maps *out) {
    if (!out || !compound(input) || !input->size) return false;
    mc_nbt_view root,list,v; int64_t version,next;
    if (!mc_nbt_root(input,&root) || !mc_nbt_find(&root,"Version",&v) || v.type!=3 || !integer(&root,"Version",&version) || version!=1 ||
        !mc_nbt_find(&root,"NextId",&v) || v.type!=3 || !integer(&root,"NextId",&next) || next<0 || next>UINT16_MAX ||
        !mc_nbt_find(&root,"Maps",&list) || list.type!=9 || list.size<5 || list.data[0]!=10) return false;
    mc_buf b={0}; b.data=(uint8_t *)list.data; b.len=list.size; b.pos=1;
    int32_t count=mc_get_i32(&b); if (count<0 || (unsigned)count>MC_MAX_MAPS) return false;
    mc_maps copy={0}; copy.next_id=(int32_t)next;
    if (!mc_nbt_copy(&copy.original_nbt,input)) return false;
    for (int32_t i=0;i<count;i++) {
        mc_nbt entry={0},data={0}; mc_map_info map={0}; int64_t id;
        bool okay=next_compound(&b,&entry) && mc_nbt_root(&entry,&root) && mc_nbt_find(&root,"Id",&v) && v.type==3 &&
            integer(&root,"Id",&id) && id>=INT32_MIN && id<=INT32_MAX && mc_nbt_find(&root,"Data",&v) && as_nbt(&v,&data) &&
            mc_map_info_decode((int32_t)id,&data,&map) && mc_nbt_copy(&map.original_entry_nbt,&entry) && add(&copy,&map);
        mc_map_info_free(&map); mc_nbt_free(&data); mc_nbt_free(&entry);
        if (!okay) { mc_maps_free(&copy); return false; }
    }
    if (b.failed || b.pos!=b.len || !encodable(&copy)) { mc_maps_free(&copy); return false; }
    mc_maps_free(out); *out=copy; return true;
}
bool mc_map_packet(const mc_map_info *m,unsigned x,unsigned z,unsigned width,unsigned height,mc_buf *out) {
    if (!out || !mc_map_info_valid(m) || width>128 || height>128 || x>128 || z>128 ||
        (width && (!height || x>128-width || z>128-height))) return false;
    mc_buf b={0}; mc_put_varint(&b,0x34); mc_put_varint(&b,m->id); mc_put_u8(&b,m->scale);
    mc_put_varint(&b,(int32_t)m->icon_count);
    for (size_t i=0;i<m->icon_count;i++) {
        mc_put_u8(&b,(uint8_t)((m->icons[i].type<<4)|m->icons[i].direction));
        mc_put_u8(&b,(uint8_t)m->icons[i].x); mc_put_u8(&b,(uint8_t)m->icons[i].z);
    }
    mc_put_u8(&b,(uint8_t)width);
    if (width) {
        mc_put_u8(&b,(uint8_t)height); mc_put_u8(&b,(uint8_t)x); mc_put_u8(&b,(uint8_t)z);
        mc_put_varint(&b,(int32_t)(width*height));
        for (unsigned j=0;j<height;j++) mc_put_bytes(&b,m->colors+x+(z+j)*128,width);
    }
    if (b.failed) { mc_buf_free(&b); return false; }
    mc_buf_free(out); *out=b; return true;
}
bool mc_maps_receive(mc_maps *maps,mc_buf *payload) {
    if (!payload || payload->failed || payload->pos>payload->len || !store_valid(maps)) return false;
    mc_buf in=*payload; int32_t id=mc_get_varint(&in); uint8_t scale=mc_get_u8(&in); int32_t icons=mc_get_varint(&in);
    if (in.failed || scale>4 || icons<0 || (unsigned)icons>MC_MAX_MAP_ICONS) return false;
    mc_map_info next={0}; const mc_map_info *old=mc_maps_find_const(maps,id);
    if (old && !mc_map_info_copy(&next,old)) return false;
    next.id=id; next.scale=scale; next.icon_count=(size_t)icons;
    for (size_t i=0;i<next.icon_count;i++) {
        uint8_t type=mc_get_u8(&in),x=mc_get_u8(&in),z=mc_get_u8(&in);
        next.icons[i].type=type>>4; next.icons[i].direction=type&15;
        memcpy(&next.icons[i].x,&x,1); memcpy(&next.icons[i].z,&z,1);
    }
    unsigned width=mc_get_u8(&in);
    if (width) {
        unsigned height=mc_get_u8(&in),x=mc_get_u8(&in),z=mc_get_u8(&in); int32_t length=mc_get_varint(&in);
        if (in.failed || width>128 || !height || height>128 || x>128-width || z>128-height || length!=(int32_t)(width*height)) goto failed;
        for (unsigned j=0;j<height;j++) if (!mc_get_bytes(&in,next.colors+x+(z+j)*128,width)) goto failed;
    }
    if (in.failed || in.pos!=in.len) goto failed;
    mc_maps copy={0}; if (!mc_maps_copy(&copy,maps)) goto failed;
    mc_map_info *target=mc_maps_find(&copy,id);
    if (target) { mc_map_info_free(target); *target=next; mc_map_info_init(&next); }
    else if (!add(&copy,&next)) { mc_maps_free(&copy); goto failed; }
    mc_maps_free(maps); *maps=copy; payload->pos=in.pos;
    mc_map_info_free(&next); return true;
failed:
    mc_map_info_free(&next); return false;
}
bool mc_map_pixel_rgb(uint8_t pixel,uint8_t rgb[3]) {
    /* Legacy palette numeric behavior, independent of texture/resource assets. */
    static const uint32_t colors[]={
        0,8368696,16247203,13092807,16711680,10526975,10987431,31744,16777215,
        10791096,9923917,7368816,4210943,9402184,16776437,14188339,11685080,
        6724056,15066419,8375321,15892389,5000268,10066329,5013401,8339378,
        3361970,6704179,6717235,10040115,1644825,16445005,6085589,4882687,
        55610,8476209,7340544
    };
    static const unsigned intensity[]={180,220,255,135};
    unsigned id=pixel>>2,shade=pixel&3;
    if (!rgb || id>=sizeof(colors)/sizeof(colors[0])) return false;
    uint32_t color=colors[id]; unsigned amount=intensity[shade];
    rgb[0]=(uint8_t)(((color>>16)&255)*amount/255);
    rgb[1]=(uint8_t)(((color>>8)&255)*amount/255);
    rgb[2]=(uint8_t)((color&255)*amount/255); return true;
}
bool mc_map_update_terrain(mc_map_info *map,const mc_world *world,double x,double z,int dimension,uint32_t tick,bool *changed) {
    return mc_ItemMap_survey(map,world,x,z,dimension,false,tick,changed);
}
