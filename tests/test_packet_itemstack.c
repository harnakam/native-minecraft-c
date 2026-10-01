#include "network/PacketBuffer.h"
#include "nbt/NBTTagCompound.h"
#include "nbt/NBTTagList.h"
#include "nbt/NBTTagInt.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static void from_hex(mc_buf *b,const char *hex) { mc_buf_init(b); for (size_t i=0;hex[i];i+=2) { unsigned value=0; CHECK(sscanf(hex+i,"%2x",&value)==1); mc_put_u8(b,(uint8_t)value); } CHECK(!b->failed); }
static void equals_hex(const mc_buf *b,const char *hex) { CHECK(b->len==strlen(hex)/2); for (size_t i=0;i<b->len;i++) { unsigned v; CHECK(sscanf(hex+i*2,"%2x",&v)==1); CHECK(b->data[i]==v); } }
static void read_golden(void) {
    typedef struct { const char *hex; bool present; int id; int count,damage; size_t bytes; } Golden;
    static const Golden values[]={
        {"ffff",false,0,0,0,2}, {"fffe",false,0,0,0,2}, {"8000",false,0,0,0,2},
        {"fffe01000200",false,0,0,0,2},
        {"000100000000",true,1,0,0,6}, {"000180ffff00",true,1,-128,0,6},
        {"0001ff7fff00",true,1,-1,32767,6}, {"0000ff000000",true,0,-1,0,6},
        {"7fff01000200",true,0,1,2,6}
    };
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); CHECK(h);
    for (unsigned i=0;i<sizeof(values)/sizeof(*values);i++) {
        const Golden *v=&values[i]; mc_buf b; from_hex(&b,v->hex); PacketBuffer p; CHECK(PacketBuffer_init(&p,h,&b)); ItemStack *s=NULL;
        CHECK(PacketBuffer_readItemStackFromBuffer(&p,&s)); CHECK((s!=NULL)==v->present&&b.pos==v->bytes);
        if (s) { CHECK(ItemStack_registryId(s->item)==v->id&&s->stackSize==v->count&&s->itemDamage==v->damage); CHECK(!s->stackTagCompound); }
        mc_buf_free(&b);
    }
    CHECK(!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void write_golden(void) {
    typedef struct { int id; int32_t count,damage; const char *hex; } Golden;
    static const Golden values[]={
        {1,0,0,"000100000000"}, {1,128,0,"000180000000"}, {1,-1,32767,"0001ff7fff00"},
        {1,INT32_MIN,INT32_MAX,"000100ffff00"}, {1,INT32_MAX,INT32_MAX,"0001ffffff00"},
        {276,2,-1,"011402000000"}
    };
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); CHECK(h);
    mc_buf b; mc_buf_init(&b); PacketBuffer p; CHECK(PacketBuffer_init(&p,h,&b)); CHECK(PacketBuffer_writeItemStackToBuffer(&p,NULL)); equals_hex(&b,"ffff"); mc_buf_free(&b);
    for (unsigned i=0;i<sizeof(values)/sizeof(*values);i++) { mc_buf_init(&b); CHECK(PacketBuffer_init(&p,h,&b)); const Golden *v=&values[i]; ItemStack *s=ItemStack_new(h,ItemStack_registryItem(v->id),v->count,v->damage); CHECK(s); CHECK(PacketBuffer_writeItemStackToBuffer(&p,s)); equals_hex(&b,v->hex); mc_buf_free(&b); }
    CHECK(!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void tags(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); CHECK(h); PacketBuffer p; mc_buf b; mc_buf_init(&b); CHECK(PacketBuffer_init(&p,h,&b));
    CHECK(PacketBuffer_writeNBTTagCompoundToBuffer(&p,NULL)); equals_hex(&b,"00"); mc_buf_clear(&b);
    NBTTagCompound *tag=NBTTagCompound_new(h); CHECK(tag); CHECK(PacketBuffer_writeNBTTagCompoundToBuffer(&p,tag)); equals_hex(&b,"0a000000"); mc_buf_clear(&b);
    CHECK(NBTTagCompound_setInteger_ascii(tag,"k",1)); CHECK(PacketBuffer_writeNBTTagCompoundToBuffer(&p,tag)); equals_hex(&b,"0a00000300016b0000000100");
    NBTTagCompound *decoded=NULL; CHECK(PacketBuffer_readNBTTagCompoundFromBuffer(&p,&decoded)); CHECK(decoded&&decoded!=tag&&NBTBase_equals((NBTBase *)tag,(NBTBase *)decoded)); mc_buf_clear(&b);
    ItemStack *s=ItemStack_new(h,ItemStack_registryItem(1),0,0); CHECK(s&&ItemStack_setTagCompound(s,tag)); CHECK(PacketBuffer_writeItemStackToBuffer(&p,s)); equals_hex(&b,"00010000000a00000300016b0000000100"); ItemStack *read=NULL; CHECK(PacketBuffer_readItemStackFromBuffer(&p,&read)); CHECK(read&&read!=s&&read->stackSize==0&&read->stackTagCompound!=tag);
    mc_buf_free(&b);
    /* Packet reading attaches the decoded tag without ItemSkull storage hook. */
    from_hex(&b,"018d0180000a000008000a536b756c6c4f776e65720005416c69636500"); CHECK(PacketBuffer_init(&p,h,&b)); CHECK(PacketBuffer_readItemStackFromBuffer(&p,&read)); CHECK(read&&read->itemDamage==0&&NBTString_equalsASCII(NBTTagCompound_getString_ascii(read->stackTagCompound,"SkullOwner"),"Alice")); mc_buf_free(&b);
    const uint16_t units[]={0,0xd800,0x3042}; tag=NBTTagCompound_new(h); CHECK(tag); CHECK(NBTTagCompound_setString_ascii(tag,"name",NBTString_fromUTF16(h,units,3))); mc_buf_init(&b); CHECK(PacketBuffer_init(&p,h,&b)); CHECK(PacketBuffer_writeNBTTagCompoundToBuffer(&p,tag)); equals_hex(&b,"0a00000800046e616d650008c080eda080e3818200"); decoded=NULL; CHECK(PacketBuffer_readNBTTagCompoundFromBuffer(&p,&decoded)); CHECK(decoded&&NBTBase_equals((NBTBase *)tag,(NBTBase *)decoded)); mc_buf_free(&b);
    CHECK(!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void identity_and_limits(void) {
    MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); CHECK(h); mc_buf b; mc_buf_init(&b); PacketBuffer p; CHECK(PacketBuffer_init(&p,h,&b));
    NBTTagCompound *tag=NBTTagCompound_new(h); NBTTagList *list=NBTTagList_new(h); CHECK(tag&&list); CHECK(NBTTagList_appendTag(list,(NBTBase *)NBTTagInt_new(h,7))); CHECK(NBTTagList_removeTag(list,0)); CHECK(NBTTagList_getTagType(list)==3&&NBTTagList_tagCount(list)==0);
    CHECK(NBTTagCompound_setTag_ascii(tag,"list",(NBTBase *)list)); ItemStack *s=ItemStack_new(h,ItemStack_registryItem(1),1,0); CHECK(s&&ItemStack_setTagCompound(s,tag));
    CHECK(PacketBuffer_writeItemStackToBuffer(&p,s)); CHECK(NBTTagList_getTagType(list)==0&&s->stackTagCompound==tag); /* direct original graph write, not copy */
    CHECK(PacketBuffer_writeItemStackToBuffer(&p,s)); ItemStack *first=NULL,*second=NULL;
    CHECK(PacketBuffer_readItemStackFromBuffer(&p,&first)&&PacketBuffer_readItemStackFromBuffer(&p,&second)); CHECK(first!=second&&first->stackTagCompound!=second->stackTagCompound&&first->stackTagCompound!=tag); CHECK(ItemStack_areItemStacksEqual(first,second)); mc_buf_free(&b);
    from_hex(&b,"0a00046e616d6500"); CHECK(PacketBuffer_init(&p,h,&b)); NBTTagCompound *decoded=NULL; CHECK(PacketBuffer_readNBTTagCompoundFromBuffer(&p,&decoded)&&decoded&&NBTBase_hasNoTags((NBTBase *)decoded)); CHECK(b.pos==b.len); mc_buf_free(&b);
    from_hex(&b,"0a00000700017800200000"); CHECK(PacketBuffer_init(&p,h,&b)); NBTTagCompound *unchanged=tag; CHECK(!PacketBuffer_readNBTTagCompoundFromBuffer(&p,&unchanged)&&unchanged==tag&&b.failed); CHECK(!MCObjectHeap_failed(h)); mc_buf_free(&b);
    int registered=0,damageable=0;
    for (int id=0;id<=2267;id++) { const Item *item=ItemStack_registryItem(id); if (item) { registered++; ItemStack *probe=ItemStack_new(h,item,1,0); CHECK(probe); if (ItemStack_getMaxDamage(probe)>0&&!ItemStack_getHasSubtypes(probe)) damageable++; } }
    CHECK(registered==337&&damageable==50); CHECK(!MCObjectHeap_failed(h)); MCObjectHeap_free(h);
}
static void malformed(void) {
    static const char *bad[]={"","00","0001","000101","0001010000","00010100000a0000","0001010000080000000161","00010100000a00000300016b0000"};
    for (unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        MCObjectHeap *h=MCObjectHeap_new(4*1024*1024); ItemStack *unchanged=ItemStack_new(h,ItemStack_registryItem(1),1,0),*out=unchanged; CHECK(unchanged); mc_buf b; from_hex(&b,bad[i]); PacketBuffer p; CHECK(PacketBuffer_init(&p,h,&b)); CHECK(!PacketBuffer_readItemStackFromBuffer(&p,&out)); CHECK(out==unchanged&&(b.failed||MCObjectHeap_failed(h))); mc_buf_free(&b); MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(1024*1024); ItemStack *s=ItemStack_new(h,NULL,1,2); CHECK(s); mc_buf b; mc_buf_init(&b); PacketBuffer p; CHECK(PacketBuffer_init(&p,h,&b)); CHECK(!PacketBuffer_writeItemStackToBuffer(&p,s)); CHECK(b.failed&&MCObjectHeap_failed(h)); equals_hex(&b,"0000010002"); mc_buf_free(&b); MCObjectHeap_free(h);
}
int main(void) { read_golden(); write_golden(); tags(); identity_and_limits(); malformed(); printf("PacketBuffer item/NBT source subset: %u checks passed\n",checks); return 0; }
