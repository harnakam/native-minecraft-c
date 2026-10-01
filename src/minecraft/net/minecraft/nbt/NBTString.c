#include "nbt/NBTInternal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
static const MCObjectClass stringClass={"JavaUTF16String",MCObjectHeap_plainClone,NULL,NULL};
bool NBTString_isInstance(const MCObject *object) {return object&&object->klass==&stringClass;}
NBTString *NBTString_fromUTF16(MCObjectHeap *heap,const uint16_t *units,size_t length) {
    if (!heap || (length && !units) || length>INT32_MAX || length>(SIZE_MAX-sizeof(NBTString))/sizeof(uint16_t)) {
        MCObjectHeap_fail(heap); return NULL;
    }
    NBTString *s=(NBTString*)MCObjectHeap_alloc(heap,sizeof(*s)+length*sizeof(uint16_t),&stringClass);
    if (!s) return NULL;
    s->length=length;
    if (length) memcpy(s->units,units,length*sizeof(uint16_t));
    return s;
}
NBTString *NBTString_fromASCII(MCObjectHeap *heap,const char *text) {
    if (!text) { MCObjectHeap_fail(heap); return NULL; }
    size_t n=strlen(text);
    if (n>INT32_MAX || n>(SIZE_MAX-sizeof(NBTString))/2) { MCObjectHeap_fail(heap); return NULL; }
    for (size_t i=0;i<n;i++) if ((unsigned char)text[i]>127) { MCObjectHeap_fail(heap); return NULL; }
    NBTString *s=(NBTString*)MCObjectHeap_alloc(heap,sizeof(*s)+n*2,&stringClass);
    if (!s) return NULL;
    s->length=n;
    for (size_t i=0;i<n;i++) s->units[i]=(unsigned char)text[i];
    return s;
}
static bool literalMatch(const MCObject *object,void *context) {
    const NBTString *s=(const NBTString*)object;
    return s->literal && NBTString_equalsASCII(s,(const char*)context);
}
NBTString *NBTString_literalASCII(MCObjectHeap *heap,const char *text) {
    if (!text) { MCObjectHeap_fail(heap); return NULL; }
    NBTString *s=(NBTString*)MCObjectHeap_findObject(heap,&stringClass,literalMatch,(void*)text);
    if (s) return s;
    s=NBTString_fromASCII(heap,text);
    if (s) s->literal=true;
    return s;
}
static bool nextUTF8(const unsigned char *s,size_t n,size_t *position,uint32_t *value) {
    size_t p=*position; if (p>=n) return false;
    uint32_t a=s[p++],v; unsigned more;
    if (a<128) { *value=a; *position=p; return true; }
    if (a>=0xc2 && a<=0xdf) { v=a&31u; more=1; }
    else if (a>=0xe0 && a<=0xef) { v=a&15u; more=2; }
    else if (a>=0xf0 && a<=0xf4) { v=a&7u; more=3; }
    else return false;
    if (more>n-p) return false;
    for (unsigned j=0;j<more;j++) { unsigned b=s[p++]; if ((b&0xc0u)!=0x80u) return false; v=(v<<6)|(b&63u); }
    if ((more==1 && v<128) || (more==2 && v<2048) || (more==3 && v<65536) || v>0x10ffff || (v>=0xd800 && v<=0xdfff)) return false;
    *value=v; *position=p; return true;
}
NBTString *NBTString_fromUTF8(MCObjectHeap *heap,const char *text) {
    if (!text) { MCObjectHeap_fail(heap); return NULL; }
    size_t n=strlen(text),p=0,len=0; uint32_t v;
    while (p<n) { if (!nextUTF8((const unsigned char*)text,n,&p,&v)) { MCObjectHeap_fail(heap); return NULL; } len+=v>=65536?2u:1u; }
    if (len>INT32_MAX || len>(SIZE_MAX-sizeof(NBTString))/2) { MCObjectHeap_fail(heap); return NULL; }
    NBTString *s=(NBTString*)MCObjectHeap_alloc(heap,sizeof(*s)+len*2,&stringClass);
    if (!s) return NULL;
    s->length=len; p=0; len=0;
    while (p<n) { if (!nextUTF8((const unsigned char*)text,n,&p,&v)) return NULL;
        if (v<65536) s->units[len++]=(uint16_t)v;
        else { v-=65536; s->units[len++]=(uint16_t)(0xd800+(v>>10)); s->units[len++]=(uint16_t)(0xdc00+(v&1023)); }
    }
    return s;
}
size_t NBTString_length(const NBTString *s) { return s?s->length:0; }
const uint16_t *NBTString_units(const NBTString *s) { return s?s->units:NULL; }
bool NBTString_equals(const NBTString *a,const NBTString *b) {
    if (a==b) return true;
    return a && b && a->length==b->length && (!a->length || memcmp(a->units,b->units,a->length*2)==0);
}
bool NBTString_equalsASCII(const NBTString *s,const char *text) {
    if (!s || !text) return !s && !text;
    size_t n=strlen(text); if (s->length!=n) return false;
    for (size_t i=0;i<n;i++) if ((unsigned char)text[i]>127 || s->units[i]!=(unsigned char)text[i]) return false;
    return true;
}
int32_t NBTString_hashCode(const NBTString *s) {
    uint32_t h=0; if (s) for (size_t i=0;i<s->length;i++) h=h*31u+s->units[i];
    return nbt_i32(h);
}
static bool scalar(const NBTString *s,size_t *index,uint32_t *value) {
    uint32_t a=s->units[(*index)++];
    if (!a) return false;
    if (a>=0xd800 && a<=0xdbff) {
        if (*index>=s->length) return false;
        uint32_t b=s->units[(*index)++]; if (b<0xdc00 || b>0xdfff) return false;
        a=0x10000+((a-0xd800)<<10)+(b-0xdc00);
    } else if (a>=0xdc00 && a<=0xdfff) return false;
    *value=a; return true;
}
bool NBTString_toUTF8(const NBTString *s,char *output,size_t capacity) {
    if (!s || !output || !capacity) return false;
    size_t i=0,n=0; uint32_t v;
    while (i<s->length) {
        if (!scalar(s,&i,&v)) return false;
        size_t bytes=v<128?1u:v<2048?2u:v<65536?3u:4u;
        if (bytes>capacity-1-n) return false;
        n+=bytes;
    }
    i=0; n=0;
    while (i<s->length) {
        if (!scalar(s,&i,&v)) return false;
        if (v<128) output[n++]=(char)v;
        else if (v<2048) { output[n++]=(char)(0xc0|(v>>6)); output[n++]=(char)(0x80|(v&63)); }
        else if (v<65536) { output[n++]=(char)(0xe0|(v>>12)); output[n++]=(char)(0x80|((v>>6)&63)); output[n++]=(char)(0x80|(v&63)); }
        else { output[n++]=(char)(0xf0|(v>>18)); output[n++]=(char)(0x80|((v>>12)&63)); output[n++]=(char)(0x80|((v>>6)&63)); output[n++]=(char)(0x80|(v&63)); }
    }
    output[n]=0; return true;
}
NBTString *nbt_readUTF(MCObjectHeap *heap,mc_buf *input) {
    uint16_t bytes=(uint16_t)mc_get_i16(input);
    if (input->failed || input->pos>input->len || bytes>input->len-input->pos) { input->failed=true; return NULL; }
    const uint8_t *data=input->data+input->pos; input->pos+=bytes;
    uint16_t *units=bytes?malloc((size_t)bytes*2):NULL;
    if (bytes && !units) { MCObjectHeap_fail(heap); input->failed=true; return NULL; }
    size_t p=0,n=0; bool good=true;
    while (p<bytes) {
        uint32_t a=data[p++];
        if (a<128) units[n++]=(uint16_t)a;
        else if ((a&0xe0)==0xc0) {
            if (p>=bytes || (data[p]&0xc0)!=0x80) { good=false; break; }
            units[n++]=(uint16_t)(((a&31)<<6)|(data[p++]&63));
        } else if ((a&0xf0)==0xe0) {
            if (bytes-p<2 || (data[p]&0xc0)!=0x80 || (data[p+1]&0xc0)!=0x80) { good=false; break; }
            units[n++]=(uint16_t)(((a&15)<<12)|((data[p]&63)<<6)|(data[p+1]&63)); p+=2;
        } else { good=false; break; }
    }
    NBTString *s=good?NBTString_fromUTF16(heap,units,n):NULL; free(units);
    if (!s) input->failed=true;
    return s;
}
bool nbt_writeUTF(mc_buf *output,const NBTString *text) {
    if (!text) { output->failed=true; return false; }
    size_t bytes=0;
    for (size_t i=0;i<text->length;i++) {
        unsigned v=text->units[i]; bytes+=v && v<128?1u:v<2048?2u:3u;
        if (bytes>UINT16_MAX) { output->failed=true; return false; }
    }
    mc_put_i16(output,nbt_i16((uint16_t)bytes));
    for (size_t i=0;i<text->length && !output->failed;i++) {
        unsigned v=text->units[i];
        if (v && v<128) mc_put_u8(output,(uint8_t)v);
        else if (v<2048) { mc_put_u8(output,(uint8_t)(0xc0|(v>>6))); mc_put_u8(output,(uint8_t)(0x80|(v&63))); }
        else { mc_put_u8(output,(uint8_t)(0xe0|(v>>12))); mc_put_u8(output,(uint8_t)(0x80|((v>>6)&63))); mc_put_u8(output,(uint8_t)(0x80|(v&63))); }
    }
    return !output->failed;
}
