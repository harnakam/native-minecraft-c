#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "nbt.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
#ifdef _WIN32
#include <io.h>
#include <process.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

typedef struct { const uint8_t *data; size_t size,pos; } nbt_cursor;

static bool take(nbt_cursor *c,size_t count,const uint8_t **data) {
    if (c->pos>c->size || count>c->size-c->pos) return false;
    if (data) *data=count ? c->data+c->pos : NULL;
    c->pos+=count; return true;
}
static uint64_t big_endian(const uint8_t *data,unsigned count) {
    uint64_t value=0;
    for (unsigned i=0;i<count;i++) value=(value<<8)|data[i];
    return value;
}
/* Modified UTF-8 encodes UTF-16 code units, not Unicode scalar values. */
static bool mutf_unit(const uint8_t *data,size_t size,size_t *position,uint16_t *unit) {
    if (*position>=size) return false;
    size_t p=*position; uint8_t first=data[p++]; uint16_t value;
    if (first>=1 && first<=0x7f) value=first;
    else if (first>=0xc0 && first<=0xdf) {
        if (p>=size || (data[p]&0xc0)!=0x80) return false;
        value=(uint16_t)(((first&31u)<<6)|(data[p++]&63u));
        if (value<128 && !(first==0xc0 && value==0)) return false;
    } else if (first>=0xe0 && first<=0xef) {
        if (size-p<2 || (data[p]&0xc0)!=0x80 || (data[p+1]&0xc0)!=0x80) return false;
        value=(uint16_t)(((first&15u)<<12)|((data[p]&63u)<<6)|(data[p+1]&63u)); p+=2;
        if (value<2048) return false;
    } else return false;
    *position=p; *unit=value; return true;
}
static bool mutf_valid(const uint8_t *data,size_t size) {
    for (size_t p=0;p<size;) { uint16_t unit; if (!mutf_unit(data,size,&p,&unit)) return false; }
    return true;
}
static bool string_payload(nbt_cursor *c,const uint8_t **data,size_t *size) {
    const uint8_t *length,*string;
    if (!take(c,2,&length)) return false;
    size_t count=(size_t)big_endian(length,2);
    if (!take(c,count,&string) || !mutf_valid(string,count)) return false;
    if (data) *data=string;
    if (size) *size=count;
    return true;
}
static bool count_payload(nbt_cursor *c,size_t *count) {
    const uint8_t *data;
    if (!take(c,4,&data) || (data[0]&0x80)) return false;
    *count=(size_t)big_endian(data,4); return true;
}
static bool payload(nbt_cursor *c,uint8_t type,unsigned depth) {
    if (type>11 || depth>MC_NBT_MAX_DEPTH) return false;
    switch (type) {
        case 0: return true;
        case 1: return take(c,1,NULL);
        case 2: return take(c,2,NULL);
        case 3: case 5: return take(c,4,NULL);
        case 4: case 6: return take(c,8,NULL);
        case 7: case 11: {
            size_t count;
            if (!count_payload(c,&count)) return false;
            size_t width=type==7 ? 1u : 4u;
            if (count>(c->size-c->pos)/width) return false;
            return take(c,count*width,NULL);
        }
        case 8: return string_payload(c,NULL,NULL);
        case 9: {
            const uint8_t *element; size_t count;
            if (!take(c,1,&element) || *element>11 || !count_payload(c,&count)) return false;
            if (*element==0) return count==0;
            /* Every non-End payload needs at least one byte. Bound work before looping. */
            if (count>c->size-c->pos) return false;
            for (size_t i=0;i<count;i++) if (!payload(c,*element,depth+1)) return false;
            return true;
        }
        case 10:
            for (;;) {
                const uint8_t *child;
                if (!take(c,1,&child) || *child>11) return false;
                if (!*child) return true;
                if (!string_payload(c,NULL,NULL) || !payload(c,*child,depth+1)) return false;
            }
        default: return false;
    }
}
static bool named_root(nbt_cursor *c,mc_nbt_view *root) {
    const uint8_t *type;
    if (!take(c,1,&type) || *type>11) return false;
    if (*type && !string_payload(c,NULL,NULL)) return false;
    size_t start=c->pos;
    if (!payload(c,*type,1)) return false;
    if (root) {
        root->type=*type; root->data=c->pos==start ? NULL : c->data+start;
        root->size=c->pos-start;
    }
    return true;
}
bool mc_nbt_validate(const void *data,size_t size) {
    if (!data || !size || size>MC_NBT_MAX_BYTES) return false;
    nbt_cursor c={(const uint8_t *)data,size,0};
    return named_root(&c,NULL) && c.pos==size;
}
static bool owned_valid(const mc_nbt *nbt) {
    return nbt && (nbt->size ? mc_nbt_validate(nbt->data,nbt->size) : nbt->data==NULL);
}
static bool view_valid(const mc_nbt_view *view) {
    if (!view || view->type>11 || view->size>MC_NBT_MAX_BYTES || (view->size && !view->data)) return false;
    nbt_cursor c={view->data,view->size,0};
    return payload(&c,view->type,1) && c.pos==c.size;
}
void mc_nbt_init(mc_nbt *nbt) { if (nbt) { nbt->data=NULL; nbt->size=0; } }
void mc_nbt_free(mc_nbt *nbt) { if (nbt) { free(nbt->data); mc_nbt_init(nbt); } }
bool mc_nbt_copy(mc_nbt *destination,const mc_nbt *source) {
    if (!destination || !owned_valid(source)) return false;
    if (destination==source) return true;
    uint8_t *copy=NULL;
    if (source->size) {
        copy=malloc(source->size); if (!copy) return false;
        memcpy(copy,source->data,source->size);
    }
    mc_nbt_free(destination); destination->data=copy; destination->size=source->size; return true;
}
bool mc_nbt_read(mc_buf *input,mc_nbt *output) {
    if (!input) return false;
    if (!output || input->failed || input->pos>input->len || !input->data || input->pos==input->len) {
        input->failed=true; return false;
    }
    size_t remaining=input->len-input->pos;
    nbt_cursor c={input->data+input->pos,remaining>MC_NBT_MAX_BYTES ? MC_NBT_MAX_BYTES : remaining,0};
    if (!named_root(&c,NULL)) { input->failed=true; return false; }
    uint8_t *copy=NULL; size_t size=c.data[0] ? c.pos : 0;
    if (size) {
        copy=malloc(size);
        if (!copy) { input->failed=true; return false; }
        memcpy(copy,c.data,size);
    }
    mc_nbt_free(output); output->data=copy; output->size=size; input->pos+=c.pos; return true;
}
bool mc_nbt_write(mc_buf *output,const mc_nbt *nbt) {
    if (!output) return false;
    if (!owned_valid(nbt)) { output->failed=true; return false; }
    if (nbt->size) mc_put_bytes(output,nbt->data,nbt->size);
    else mc_put_u8(output,0);
    return !output->failed;
}
bool mc_nbt_root(const mc_nbt *nbt,mc_nbt_view *view) {
    if (!view || !owned_valid(nbt)) return false;
    mc_nbt_view result={0,NULL,0};
    if (nbt->size) {
        nbt_cursor c={nbt->data,nbt->size,0};
        if (!named_root(&c,&result)) return false;
    }
    *view=result; return true;
}
static bool mutf_scalar(const uint8_t *data,size_t size,size_t *position,uint32_t *scalar) {
    uint16_t first;
    if (!mutf_unit(data,size,position,&first)) return false;
    if (first>=0xd800 && first<=0xdbff) {
        uint16_t second;
        if (!mutf_unit(data,size,position,&second) || second<0xdc00 || second>0xdfff) return false;
        *scalar=0x10000u+((uint32_t)(first-0xd800)<<10)+(second-0xdc00u);
    } else {
        if (first>=0xdc00 && first<=0xdfff) return false;
        *scalar=first;
    }
    return true;
}
static bool utf8_scalar(const uint8_t *data,size_t size,size_t *position,uint32_t *scalar) {
    if (*position>=size) return false;
    size_t p=*position; uint32_t value=data[p++]; unsigned continuation;
    if (value<128) continuation=0;
    else if (value>=0xc2 && value<=0xdf) { value&=31; continuation=1; }
    else if (value>=0xe0 && value<=0xef) { value&=15; continuation=2; }
    else if (value>=0xf0 && value<=0xf4) { value&=7; continuation=3; }
    else return false;
    if (continuation>size-p) return false;
    for (unsigned i=0;i<continuation;i++) {
        if ((data[p]&0xc0)!=0x80) return false;
        value=(value<<6)|(data[p++]&63u);
    }
    if ((continuation==1 && value<128) || (continuation==2 && value<2048) ||
        (continuation==3 && value<65536) || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return false;
    *position=p; *scalar=value; return true;
}
static bool name_matches(const uint8_t *data,size_t size,const char *name,size_t name_size) {
    size_t a=0,b=0;
    while (a<size && b<name_size) {
        uint32_t left,right;
        if (!mutf_scalar(data,size,&a,&left) || !utf8_scalar((const uint8_t *)name,name_size,&b,&right) || left!=right) return false;
    }
    return a==size && b==name_size;
}
bool mc_nbt_find(const mc_nbt_view *compound,const char *name,mc_nbt_view *view) {
    if (!view || !name || !compound || compound->type!=10 || !view_valid(compound)) return false;
    size_t name_size=strlen(name);
    if (name_size>3u*UINT16_MAX) return false;
    nbt_cursor c={compound->data,compound->size,0}; bool found=false; mc_nbt_view result={0,NULL,0};
    while (c.pos<c.size) {
        const uint8_t *type,*tag_name; size_t tag_name_size;
        if (!take(&c,1,&type)) return false;
        if (!*type) break;
        if (!string_payload(&c,&tag_name,&tag_name_size)) return false;
        size_t start=c.pos;
        if (!payload(&c,*type,2)) return false;
        if (name_matches(tag_name,tag_name_size,name,name_size)) {
            result.type=*type; result.data=c.data+start; result.size=c.pos-start; found=true;
        }
    }
    if (found) *view=result;
    return found;
}
bool mc_nbt_list_get(const mc_nbt_view *list,size_t index,mc_nbt_view *view) {
    if (!view || !list || list->type!=9 || !view_valid(list)) return false;
    nbt_cursor c={list->data,list->size,0}; const uint8_t *type; size_t count;
    if (!take(&c,1,&type) || !count_payload(&c,&count) || index>=count) return false;
    for (size_t i=0;i<index;i++) if (!payload(&c,*type,2)) return false;
    size_t start=c.pos;
    if (!payload(&c,*type,2)) return false;
    mc_nbt_view result={*type,c.data+start,c.pos-start}; *view=result; return true;
}
bool mc_nbt_get_integer(const mc_nbt_view *view,int64_t *value) {
    if (!value || !view || view->type<1 || view->type>4 || !view_valid(view)) return false;
    unsigned bytes=1u<<(view->type-1); uint64_t raw=big_endian(view->data,bytes); int64_t result;
    if (bytes==8) memcpy(&result,&raw,8);
    else if (raw&(UINT64_C(1)<<(bytes*8-1))) result=(int64_t)raw-(INT64_C(1)<<(bytes*8));
    else result=(int64_t)raw;
    *value=result; return true;
}
bool mc_nbt_get_number(const mc_nbt_view *view,double *value) {
    if (!value || !view || view->type<1 || view->type>6 || !view_valid(view)) return false;
    double result;
    if (view->type<=4) { int64_t integer; if (!mc_nbt_get_integer(view,&integer)) return false; result=(double)integer; }
    else if (view->type==5) { uint32_t bits=(uint32_t)big_endian(view->data,4); float number; memcpy(&number,&bits,4); result=number; }
    else { uint64_t bits=big_endian(view->data,8); memcpy(&result,&bits,8); }
    *value=result; return true;
}
static size_t utf8_write(uint32_t scalar,char *output) {
    size_t count;
    if (scalar<128) count=1;
    else if (scalar<2048) count=2;
    else if (scalar<65536) count=3;
    else count=4;
    if (output) {
        if (count==1) output[0]=(char)scalar;
        else {
            uint32_t remaining=scalar;
            for (size_t i=count-1;i>0;i--) { output[i]=(char)(0x80u|(remaining&63u)); remaining>>=6; }
            output[0]=(char)((count==2 ? 0xc0u : count==3 ? 0xe0u : 0xf0u)|remaining);
        }
    }
    return count;
}
bool mc_nbt_get_string(const mc_nbt_view *view,char *output,size_t capacity) {
    if (!output || !capacity || !view || view->type!=8 || !view_valid(view)) return false;
    nbt_cursor c={view->data,view->size,0}; const uint8_t *data; size_t size;
    if (!string_payload(&c,&data,&size)) return false;
    size_t needed=0;
    for (size_t p=0;p<size;) {
        uint32_t scalar;
        if (!mutf_scalar(data,size,&p,&scalar) || !scalar) return false;
        needed+=utf8_write(scalar,NULL);
    }
    if (needed>=capacity) return false;
    char local[256]; char *decoded=needed<sizeof(local) ? local : malloc(needed+1);
    if (!decoded) return false;
    size_t written=0;
    for (size_t p=0;p<size;) {
        uint32_t scalar;
        if (!mutf_scalar(data,size,&p,&scalar)) { if (decoded!=local) free(decoded); return false; }
        written+=utf8_write(scalar,decoded+written);
    }
    decoded[written]=0; memmove(output,decoded,written+1);
    if (decoded!=local) free(decoded);
    return true;
}

/* Compound equality follows Java's name-to-value map, including last-name-wins. */
typedef struct {
    const uint8_t *name;
    size_t name_size,ordinal;
    mc_nbt_view value;
} nbt_entry;
static int entry_name_compare(const nbt_entry *a,const nbt_entry *b) {
    size_t common=a->name_size<b->name_size ? a->name_size : b->name_size;
    int order=common ? memcmp(a->name,b->name,common) : 0;
    if (order) return order;
    return a->name_size<b->name_size ? -1 : a->name_size>b->name_size;
}
static int entry_compare(const void *left,const void *right) {
    const nbt_entry *a=left,*b=right; int order=entry_name_compare(a,b);
    if (order) return order;
    return a->ordinal<b->ordinal ? -1 : a->ordinal>b->ordinal;
}
static bool compound_entries(const mc_nbt_view *view,nbt_entry **entries,size_t *count) {
    nbt_cursor c={view->data,view->size,0}; size_t total=0;
    while (c.pos<c.size) {
        const uint8_t *type;
        if (!take(&c,1,&type)) return false;
        if (!*type) break;
        if (!string_payload(&c,NULL,NULL) || !payload(&c,*type,2)) return false;
        total++;
    }
    if (total>SIZE_MAX/sizeof(nbt_entry)) return false;
    nbt_entry *result=total ? malloc(total*sizeof(*result)) : NULL;
    if (total && !result) return false;
    c.pos=0;
    for (size_t i=0;i<total;i++) {
        const uint8_t *type;
        if (!take(&c,1,&type) || !string_payload(&c,&result[i].name,&result[i].name_size)) { free(result); return false; }
        size_t start=c.pos;
        if (!payload(&c,*type,2)) { free(result); return false; }
        result[i].ordinal=i; result[i].value.type=*type;
        result[i].value.data=c.data+start; result[i].value.size=c.pos-start;
    }
    if (total>1) qsort(result,total,sizeof(*result),entry_compare);
    size_t unique=0;
    for (size_t i=0;i<total;) {
        size_t last=i;
        while (last+1<total && !entry_name_compare(&result[i],&result[last+1])) last++;
        result[unique++]=result[last]; i=last+1;
    }
    *entries=result; *count=unique; return true;
}
static bool value_equal(const mc_nbt_view *a,const mc_nbt_view *b,unsigned depth) {
    if (depth>MC_NBT_MAX_DEPTH || a->type!=b->type) return false;
    if (a->type==5 || a->type==6) {
        double left,right;
        return mc_nbt_get_number(a,&left) && mc_nbt_get_number(b,&right) && left==right;
    }
    if (a->type!=9 && a->type!=10)
        return a->size==b->size && (!a->size || !memcmp(a->data,b->data,a->size));
    if (a->type==9) {
        nbt_cursor left={a->data,a->size,0},right={b->data,b->size,0};
        const uint8_t *lt,*rt; size_t lc,rc;
        if (!take(&left,1,&lt) || !take(&right,1,&rt) || *lt!=*rt ||
            !count_payload(&left,&lc) || !count_payload(&right,&rc) || lc!=rc) return false;
        for (size_t i=0;i<lc;i++) {
            size_t ls=left.pos,rs=right.pos;
            if (!payload(&left,*lt,depth+1) || !payload(&right,*rt,depth+1)) return false;
            mc_nbt_view lv={*lt,left.data+ls,left.pos-ls},rv={*rt,right.data+rs,right.pos-rs};
            if (!value_equal(&lv,&rv,depth+1)) return false;
        }
        return true;
    }
    nbt_entry *left=NULL,*right=NULL; size_t lc=0,rc=0; bool equal=false;
    if (!compound_entries(a,&left,&lc) || !compound_entries(b,&right,&rc) || lc!=rc) goto done;
    for (size_t i=0;i<lc;i++)
        if (entry_name_compare(&left[i],&right[i]) || !value_equal(&left[i].value,&right[i].value,depth+1)) goto done;
    equal=true;
done:
    free(left); free(right); return equal;
}
bool mc_nbt_equal(const mc_nbt *a,const mc_nbt *b) {
    mc_nbt_view left,right;
    if (!mc_nbt_root(a,&left) || !mc_nbt_root(b,&right)) return false;
    /* Java collection equality preserves object identity, even with NaN tags. */
    if (a==b && (left.type==9 || left.type==10)) return true;
    return value_equal(&left,&right,1);
}

static void diagnostic(char *error,size_t size,const char *message) {
    if (error && size) snprintf(error,size,"%s",message);
}
static void system_diagnostic(char *error,size_t size,const char *message,int code) {
    if (error && size) snprintf(error,size,"%s: %s",message,strerror(code));
}
bool mc_nbt_load_gzip(mc_nbt *output,const char *path,char *error,size_t error_size) {
    diagnostic(error,error_size,"");
    if (!output || !path || !*path) { diagnostic(error,error_size,"Invalid NBT load arguments"); return false; }
    FILE *file=fopen(path,"rb");
    if (!file) { system_diagnostic(error,error_size,"Cannot open NBT file",errno); return false; }
    const size_t compressed_limit=2u*MC_NBT_MAX_BYTES;
    uint8_t *compressed=malloc(compressed_limit+1u),*raw=NULL; bool success=false;
    if (!compressed) { diagnostic(error,error_size,"Cannot allocate compressed NBT buffer"); goto close_file; }
    size_t compressed_size=fread(compressed,1,compressed_limit+1u,file);
    if (ferror(file)) { system_diagnostic(error,error_size,"Cannot read NBT file",errno); goto close_file; }
    if (compressed_size>compressed_limit) { diagnostic(error,error_size,"Compressed NBT file exceeds size limit"); goto close_file; }
    if (fclose(file)!=0) { file=NULL; system_diagnostic(error,error_size,"Cannot close NBT file",errno); goto done; }
    file=NULL;
    raw=malloc(MC_NBT_MAX_BYTES+1u);
    if (!raw) { diagnostic(error,error_size,"Cannot allocate inflated NBT buffer"); goto done; }
    z_stream stream; memset(&stream,0,sizeof(stream));
    int result=inflateInit2(&stream,15+16);
    if (result!=Z_OK) { diagnostic(error,error_size,"Cannot initialize gzip decoder"); goto done; }
    stream.next_in=compressed; stream.avail_in=(uInt)compressed_size;
    stream.next_out=raw; stream.avail_out=MC_NBT_MAX_BYTES+1u;
    result=inflate(&stream,Z_FINISH);
    size_t raw_size=(size_t)stream.total_out,consumed=(size_t)stream.total_in;
    (void)inflateEnd(&stream);
    if (raw_size>MC_NBT_MAX_BYTES) { diagnostic(error,error_size,"Inflated NBT exceeds size limit"); goto done; }
    if (result!=Z_STREAM_END) { diagnostic(error,error_size,"Invalid or truncated gzip NBT file"); goto done; }
    if (consumed!=compressed_size) { diagnostic(error,error_size,"Trailing data after gzip NBT stream"); goto done; }
    if (!mc_nbt_validate(raw,raw_size)) { diagnostic(error,error_size,"Invalid NBT root in gzip file"); goto done; }
    mc_nbt_free(output);
    if (raw[0]) {
        uint8_t *exact=realloc(raw,raw_size);
        output->data=exact ? exact : raw; output->size=raw_size; raw=NULL;
    }
    success=true; goto done;
close_file:
    if (file) (void)fclose(file);
done:
    free(raw); free(compressed); return success;
}

static bool commit_descriptor(int descriptor) {
#ifdef _WIN32
    return _commit(descriptor)==0;
#else
    return fsync(descriptor)==0;
#endif
}
static void close_descriptor(int descriptor) {
#ifdef _WIN32
    (void)_close(descriptor);
#else
    (void)close(descriptor);
#endif
}
static int create_exclusive(const char *path) {
#ifdef _WIN32
    return _open(path,_O_CREAT|_O_EXCL|_O_WRONLY|_O_BINARY,_S_IREAD|_S_IWRITE);
#else
    return open(path,O_CREAT|O_EXCL|O_WRONLY,0600);
#endif
}
static bool replace_file(const char *temporary,const char *path,char *error,size_t error_size) {
#ifdef _WIN32
    if (MoveFileExA(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return true;
    if (error && error_size) snprintf(error,error_size,"Cannot replace NBT file (Windows error %lu)",(unsigned long)GetLastError());
#else
    if (rename(temporary,path)==0) return true;
    system_diagnostic(error,error_size,"Cannot replace NBT file",errno);
#endif
    return false;
}
bool mc_nbt_save_gzip(const mc_nbt *nbt,const char *path,char *error,size_t error_size) {
    diagnostic(error,error_size,"");
    if (!owned_valid(nbt) || !path || !*path) { diagnostic(error,error_size,"Invalid NBT save arguments"); return false; }
    uint8_t end=0; const uint8_t *raw=nbt->size ? nbt->data : &end; size_t raw_size=nbt->size ? nbt->size : 1u;
    z_stream stream; memset(&stream,0,sizeof(stream));
    if (deflateInit2(&stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,15+16,8,Z_DEFAULT_STRATEGY)!=Z_OK) {
        diagnostic(error,error_size,"Cannot initialize gzip encoder"); return false;
    }
    uLong bound=deflateBound(&stream,(uLong)raw_size); uint8_t *compressed=malloc((size_t)bound);
    if (!compressed) { (void)deflateEnd(&stream); diagnostic(error,error_size,"Cannot allocate gzip encoder buffer"); return false; }
    stream.next_in=(Bytef *)raw; stream.avail_in=(uInt)raw_size;
    stream.next_out=compressed; stream.avail_out=(uInt)bound;
    int result=deflate(&stream,Z_FINISH); size_t compressed_size=(size_t)stream.total_out;
    (void)deflateEnd(&stream);
    if (result!=Z_STREAM_END) { free(compressed); diagnostic(error,error_size,"Cannot encode gzip NBT file"); return false; }
    size_t path_size=strlen(path);
    if (path_size>SIZE_MAX-80) { free(compressed); diagnostic(error,error_size,"NBT file path is too long"); return false; }
    char *temporary=malloc(path_size+80);
    if (!temporary) { free(compressed); diagnostic(error,error_size,"Cannot allocate temporary NBT path"); return false; }
    static atomic_uint sequence=ATOMIC_VAR_INIT(0);
#ifdef _WIN32
    unsigned long process=(unsigned long)_getpid();
#else
    unsigned long process=(unsigned long)getpid();
#endif
    int descriptor=-1;
    for (unsigned attempt=0;attempt<128;attempt++) {
        unsigned ticket=atomic_fetch_add_explicit(&sequence,1,memory_order_relaxed);
        snprintf(temporary,path_size+80,"%s.tmp.%lu.%u",path,process,ticket);
        descriptor=create_exclusive(temporary);
        if (descriptor>=0 || errno!=EEXIST) break;
    }
    bool success=false;
    if (descriptor<0) { system_diagnostic(error,error_size,"Cannot create temporary NBT file",errno); goto done; }
#ifdef _WIN32
    FILE *file=_fdopen(descriptor,"wb");
#else
    FILE *file=fdopen(descriptor,"wb");
#endif
    if (!file) {
        int code=errno; close_descriptor(descriptor); system_diagnostic(error,error_size,"Cannot open temporary NBT stream",code); goto remove_temporary;
    }
    if (fwrite(compressed,1,compressed_size,file)!=compressed_size || fflush(file)!=0 || !commit_descriptor(descriptor)) {
        int code=errno; (void)fclose(file); system_diagnostic(error,error_size,"Cannot flush temporary NBT file",code); goto remove_temporary;
    }
    if (fclose(file)!=0) { system_diagnostic(error,error_size,"Cannot close temporary NBT file",errno); goto remove_temporary; }
    success=replace_file(temporary,path,error,error_size);
remove_temporary:
    if (!success) (void)remove(temporary);
done:
    free(temporary); free(compressed); return success;
}
