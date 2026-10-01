#include "entity/SharedMonsterAttributes.h"
#include "entity/ai/attributes/RangedAttribute.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do {checks++; if (!(x)) {fprintf(stderr,"attribute check %u line %d: %s\n",checks,__LINE__,#x);exit(1);}} while (0)
static IAttribute *attribute(MCObjectHeap *h,const char *name,IAttribute *parent,double def) {
    RangedAttribute *a=RangedAttribute_new(h,parent,NBTString_fromASCII(h,name),def,-1e300,1e300);CHECK(a);return (IAttribute *)a;
}
static AttributeModifier *modifier(MCObjectHeap *h,int64_t key,const char *name,double amount,int32_t op) {
    AttributeModifier *m=AttributeModifier_new(h,NativeJavaUUID_new(h,0,key),NBTString_fromASCII(h,name),amount,op);CHECK(m);return m;
}
static void source_values_and_live_views(void) {
    MCObjectHeap *heap=MCObjectHeap_new(4u*1024u*1024u);CHECK(heap);
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,heap));
    NBTString *name=NBTString_fromASCII(heap,"test.health");CHECK(name);
    RangedAttribute *attribute=RangedAttribute_new(heap,NULL,name,20,0,1024);
    CHECK(attribute);
    CHECK(IAttribute_clampValue((IAttribute *)attribute,-10)==0);
    CHECK(IAttribute_clampValue((IAttribute *)attribute,2000)==1024);
    CHECK(isnan(IAttribute_clampValue((IAttribute *)attribute,NAN)));
    SharedMonsterAttributes *shared=SharedMonsterAttributes_get(heap);CHECK(shared);
    CHECK(SharedMonsterAttributes_get(heap)==shared);
    MCObject *tiny=MCObjectHeap_alloc(heap,sizeof(MCObject),shared->object.klass);CHECK(tiny);CHECK(SharedMonsterAttributes_get(heap)==shared);
    ServersideAttributeMap *map=ServersideAttributeMap_new(heap);CHECK(map);
    IAttributeInstance *health=BaseAttributeMap_registerAttribute(&map->base,shared->maxHealth);CHECK(health);
    CHECK(IAttributeInstance_getAttributeValue(health)==20);
    CHECK(BaseAttributeMap_getAttributeInstanceByName(&map->base,NBTString_fromASCII(heap,"GENERIC.MAXHEALTH"))==health);
    CHECK(BaseAttributeMap_getAttributeInstanceByName(&map->base,NBTString_fromASCII(heap,"max health"))==health);
    CHECK(BaseAttributeMap_getAllAttributes(&map->base)==BaseAttributeMap_getAllAttributes(&map->base));
    CHECK(AttributeCollection_size(ServersideAttributeMap_getAttributeInstanceSet(map))==0);
    CHECK(IAttributeInstance_setBaseValue(health,20));
    CHECK(AttributeCollection_size(ServersideAttributeMap_getAttributeInstanceSet(map))==0);
    AttributeModifier *add=modifier(heap,1,"add",4,0),*percent=modifier(heap,2,"percent",0.5,1),*multiply=modifier(heap,3,"multiply",0.5,2);
    CHECK(IAttributeInstance_applyModifier(health,add));CHECK(IAttributeInstance_applyModifier(health,percent));CHECK(IAttributeInstance_applyModifier(health,multiply));
    CHECK(IAttributeInstance_getAttributeValue(health)==54);
    CHECK(AttributeCollection_size(ServersideAttributeMap_getAttributeInstanceSet(map))==1);
    CHECK(AttributeCollection_contains(ServersideAttributeMap_getAttributeInstanceSet(map),(MCObject *)health));
    CHECK(AttributeCollection_size(ServersideAttributeMap_getWatchedAttributes(map))==1);
    CHECK(IAttributeInstance_getModifiersByOperation(health,-1)==NULL&&!MCObjectHeap_failed(heap));
    AttributeCollection *live=IAttributeInstance_getModifiersByOperation(health,0);
    CHECK(live==IAttributeInstance_getModifiersByOperation(health,0));
    CHECK(AttributeCollection_remove(live,(MCObject *)add));
    CHECK(IAttributeInstance_getModifier(health,add->id)==add);
    CHECK(IAttributeInstance_getAttributeValue(health)==54); /* direct Set mutation does not invalidate the source cache */
    CHECK(IAttributeInstance_setBaseValue(health,22));
    CHECK(IAttributeInstance_getAttributeValue(health)==49.5);
    CHECK(IAttributeInstance_removeAllModifiers(health));CHECK(IAttributeInstance_getAttributeValue(health)==22);
    CHECK(IAttributeInstance_getModifier(health,add->id)==add); /* orphan UUID entry survives removeAll's operation union */
    CHECK(IAttributeInstance_removeModifier(health,add));CHECK(IAttributeInstance_getModifier(health,add->id)==NULL);
    CHECK(IAttributeInstance_applyModifier(health,modifier(heap,20,"percent-a",0.5,1)));
    CHECK(IAttributeInstance_applyModifier(health,modifier(heap,21,"percent-b",0.25,1)));
    CHECK(IAttributeInstance_getAttributeValue(health)==38.5); /* both additions use the same post-operation-0 base */
    CHECK(IAttributeInstance_removeAllModifiers(health));CHECK(IAttributeInstance_getAttributeValue(health)==22);
    AttributeCollection *all=BaseAttributeMap_getAllAttributes(&map->base);
    CHECK(AttributeCollection_remove(all,(MCObject *)health));CHECK(AttributeCollection_size(all)==0);
    CHECK(BaseAttributeMap_getAttributeInstance(&map->base,shared->maxHealth)==health); /* source maps are separate */
    CHECK(BaseAttributeMap_getAttributeInstanceByName(&map->base,NBTString_fromASCII(heap,"generic.maxHealth"))==NULL);
    CHECK(BaseAttributeMap_getAttributeInstanceByName(&map->base,NBTString_fromASCII(heap,"Max Health"))==health); /* description map remains */
    MCObjectRootScope_end(&scope);MCObjectHeap_free(heap);
}
static void inherited_modifier_identity_and_cache(void) {
    MCObjectHeap *h=MCObjectHeap_new(4u*1024u*1024u);CHECK(h);MCObjectRootScope s={0};CHECK(MCObjectRootScope_begin(&s,h));
    ServersideAttributeMap *map=ServersideAttributeMap_new(h);CHECK(map);
    IAttribute *parent=attribute(h,"parent",NULL,10),*child=attribute(h,"child",parent,20),*grandchild=attribute(h,"grandchild",child,30);
    CHECK(BaseAttribute_setShouldWatch((BaseAttribute *)grandchild,true));
    IAttributeInstance *p=BaseAttributeMap_registerAttribute(&map->base,parent),*c=BaseAttributeMap_registerAttribute(&map->base,child),*g=BaseAttributeMap_registerAttribute(&map->base,grandchild);CHECK(p&&c&&g);
    AttributeModifier *pa=modifier(h,17,"parent-name",3,0),*ca=modifier(h,17,"child-name",7,0);
    CHECK(IAttributeInstance_applyModifier(p,pa));CHECK(IAttributeInstance_applyModifier(c,ca));
    CHECK(IAttributeInstance_getAttributeValue(c)==27); /* child set wins equal UUID across ancestor union */
    CHECK(IAttributeInstance_getAttributeValue(g)==37); /* immediate ancestor visited before root */
    CHECK(IAttributeInstance_setBaseValue(p,500));CHECK(IAttributeInstance_getAttributeValue(g)==37); /* inherited base is not added */
    CHECK(IAttributeInstance_removeModifier(c,ca));CHECK(IAttributeInstance_getAttributeValue(c)==23);CHECK(IAttributeInstance_getAttributeValue(g)==33);
    CHECK(IAttributeInstance_setBaseValue(p,NAN));CHECK(((ModifiableAttributeInstance *)g)->needsUpdate);
    CHECK(IAttributeInstance_getAttributeValue(g)==33);CHECK(!((ModifiableAttributeInstance *)g)->needsUpdate);
    CHECK(AttributeCollection_size(ServersideAttributeMap_getAttributeInstanceSet(map))==1);
    CHECK(IAttributeInstance_removeModifier(p,ca)); /* UUID equality, supplied name differs */
    CHECK(IAttributeInstance_getAttributeValue(g)==30);
    AttributeModifier *nullUuid=AttributeModifier_new(h,NULL,NBTString_fromASCII(h,"null-id"),4,0);CHECK(nullUuid);
    CHECK(IAttributeInstance_applyModifier(c,nullUuid));CHECK(IAttributeInstance_getModifier(c,NULL)==nullUuid);CHECK(IAttributeInstance_getAttributeValue(c)==24);
    CHECK(IAttributeInstance_removeModifier(c,nullUuid));CHECK(IAttributeInstance_getAttributeValue(c)==20);
    MCObjectRootScope_end(&s);MCObjectHeap_free(h);
}
static unsigned warnings;
static bool unknown(MCObject *context,NBTString *name) {(void)context;CHECK(NBTString_equalsASCII(name,"unknown"));warnings++;return true;}
static bool invalid(MCObject *context) {(void)context;warnings++;return true;}
static const SharedMonsterAttributesLogging logging={unknown,invalid};
static void nbt_saved_flags_and_duplicate_replacement(void) {
    MCObjectHeap *h=MCObjectHeap_new(4u*1024u*1024u);CHECK(h);MCObjectRootScope s={0};CHECK(MCObjectRootScope_begin(&s,h));
    SharedMonsterAttributes *shared=SharedMonsterAttributes_get(h);ServersideAttributeMap *map=ServersideAttributeMap_new(h);CHECK(shared&&map);
    IAttributeInstance *i=BaseAttributeMap_registerAttribute(&map->base,shared->maxHealth);CHECK(i);
    AttributeModifier *a=modifier(h,1,"unsaved",3,0);CHECK(AttributeModifier_setSaved(a,false));CHECK(IAttributeInstance_applyModifier(i,a));
    NBTTagCompound *tag=SharedMonsterAttributes_writeAttributeInstanceToNBT(i);CHECK(tag);
    CHECK(NBTTagCompound_hasKeyType_ascii(tag,"Modifiers",9));CHECK(NBTTagList_tagCount(NBTTagCompound_getTagList_ascii(tag,"Modifiers",10))==0);
    AttributeModifier *b=modifier(h,2,"saved",0.5,1);CHECK(IAttributeInstance_applyModifier(i,b));
    tag=SharedMonsterAttributes_writeAttributeInstanceToNBT(i);CHECK(tag);
    NBTTagList *mods=NBTTagCompound_getTagList_ascii(tag,"Modifiers",10);CHECK(NBTTagList_tagCount(mods)==1);
    NBTTagCompound *m=NBTTagList_getCompoundTagAt(mods,0);CHECK(NBTTagCompound_getLong_ascii(m,"UUIDLeast")==2);CHECK(NBTTagCompound_getInteger_ascii(m,"Operation")==1);
    CHECK(NBTTagCompound_setDouble_ascii(tag,"Base",40));CHECK(NBTTagCompound_setDouble_ascii(m,"Amount",0.25));
    NBTTagList *list=NBTTagList_new(h);CHECK(NBTTagList_appendTag(list,(NBTBase *)tag));
    CHECK(SharedMonsterAttributes_setAttributeModifiers(&map->base,list,&logging,NULL));
    AttributeModifier *replacement=IAttributeInstance_getModifier(i,b->id);CHECK(replacement&&replacement!=b&&replacement->amount==0.25);
    CHECK(IAttributeInstance_getAttributeValue(i)==53.75); /* unsaved existing modifier retained */
    NBTTagCompound *bad=NBTTagCompound_new(h);CHECK(bad);CHECK(NBTTagCompound_setString_ascii(bad,"Name",NBTString_fromASCII(h,"")));CHECK(NBTTagCompound_setInteger_ascii(bad,"Operation",99));
    unsigned before=warnings;CHECK(SharedMonsterAttributes_readAttributeModifierFromNBT(bad,&logging,NULL)==NULL);CHECK(!MCObjectHeap_failed(h));CHECK(warnings==before+1);
    NBTTagCompound *foreign=NBTTagCompound_new(h);CHECK(NBTTagCompound_setString_ascii(foreign,"Name",NBTString_fromASCII(h,"unknown")));
    NBTTagList *unknownList=NBTTagList_new(h);CHECK(NBTTagList_appendTag(unknownList,(NBTBase *)foreign));CHECK(SharedMonsterAttributes_setAttributeModifiers(&map->base,unknownList,&logging,NULL));CHECK(warnings==before+2);
    MCObjectRootScope_end(&s);MCObjectHeap_free(h);
}
static void bulk_entries_and_collection_resource_boundary(void) {
    MCObjectHeap *h=MCObjectHeap_new(4u*1024u*1024u);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    ServersideAttributeMap *map=ServersideAttributeMap_new(h);IAttribute *a=attribute(h,"target",NULL,10);IAttributeInstance *i=BaseAttributeMap_registerAttribute(&map->base,a);CHECK(i);
    AttributeModifier *old=modifier(h,1,"old",2,0),*replacement=modifier(h,1,"replacement",4,0);CHECK(IAttributeInstance_applyModifier(i,old));
    AttributeModifierMultimap *entries=AttributeModifierMultimap_new(h);CHECK(entries);
    CHECK(AttributeModifierMultimap_put(entries,NBTString_fromASCII(h,"target"),replacement));
    CHECK(AttributeModifierMultimap_put(entries,NBTString_fromASCII(h,"missing"),modifier(h,2,"ignored",99,0)));
    CHECK(BaseAttributeMap_applyAttributeModifiers(&map->base,entries));CHECK(IAttributeInstance_getModifier(i,old->id)==replacement);CHECK(IAttributeInstance_getAttributeValue(i)==14);
    CHECK(BaseAttributeMap_removeAttributeModifiers(&map->base,entries));CHECK(IAttributeInstance_getAttributeValue(i)==10);
    AttributeCollection *set=AttributeCollection_newSet(h,ATTRIBUTE_KEY_MODIFIER);CHECK(set);CHECK(AttributeCollection_add(set,(MCObject *)old));CHECK(AttributeCollection_add(set,(MCObject *)replacement));CHECK(AttributeCollection_size(set)==1);CHECK(AttributeCollection_getAt(set,0)==(MCObject *)old);
    /* Long-lived remove/reinsert uses bounded live storage, not an ever-growing
       tombstone count which would eventually reject ordinary gameplay. */
    for(int n=0;n<4200;n++){CHECK(AttributeCollection_remove(set,(MCObject *)old));CHECK(AttributeCollection_add(set,(MCObject *)old));}
    CHECK(AttributeCollection_size(set)==1&&!MCObjectHeap_failed(h));
    MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
    h=MCObjectHeap_new(4u*1024u*1024u);CHECK(h);CHECK(MCObjectRootScope_begin(&scope,h));set=AttributeCollection_newSet(h,ATTRIBUTE_KEY_MODIFIER);CHECK(set);
    for(int64_t n=1;n<=10;n++){
        int64_t id=(int64_t)(((uint64_t)n<<32)|(uint64_t)n);CHECK(AttributeCollection_add(set,(MCObject *)modifier(h,id,"collision",1,0)));
    }
    CHECK(AttributeCollection_size(set)==10);int64_t id=(int64_t)((UINT64_C(11)<<32)|11);
    CHECK(!AttributeCollection_add(set,(MCObject *)modifier(h,id,"unsupported-tree-bin",1,0)));CHECK(MCObjectHeap_failed(h));
    MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
}
static void failures_and_lifetime(void) {
    CHECK(!AttributeModifier_equals(NULL,NULL));
    for(int mode=0;mode<5;mode++) {
        MCObjectHeap *h=MCObjectHeap_new(4u*1024u*1024u);CHECK(h);MCObjectRootScope s={0};CHECK(MCObjectRootScope_begin(&s,h));
        ServersideAttributeMap *map=ServersideAttributeMap_new(h);CHECK(map);IAttribute *a=attribute(h,"name",NULL,3);
        IAttributeInstance *i=BaseAttributeMap_registerAttribute(&map->base,a);CHECK(i);
        if(mode==0){CHECK(!BaseAttributeMap_registerAttribute(&map->base,attribute(h,"NAME",NULL,7)));}
        if(mode==1){AttributeModifier *m=modifier(h,1,"one",1,0);CHECK(IAttributeInstance_applyModifier(i,m));CHECK(!IAttributeInstance_applyModifier(i,modifier(h,1,"other",9,2)));}
        if(mode==2){CHECK(!AttributeModifier_new(h,NULL,NBTString_fromASCII(h,""),1,0));}
        if(mode==3){uint16_t units[]={0x0130};CHECK(!BaseAttributeMap_getAttributeInstanceByName(&map->base,NBTString_fromUTF16(h,units,1)));}
        if(mode==4){MCObjectHeap *foreign=MCObjectHeap_new(1024*1024);CHECK(foreign);IAttribute *other=attribute(foreign,"other",NULL,1);CHECK(!BaseAttributeMap_registerAttribute(&map->base,other));MCObjectHeap_free(foreign);}
        CHECK(MCObjectHeap_failed(h));MCObjectRootScope_end(&s);MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(4u*1024u*1024u);CHECK(h);MCObjectRootScope s={0};CHECK(MCObjectRootScope_begin(&s,h));
    SharedMonsterAttributes *shared=SharedMonsterAttributes_get(h);ServersideAttributeMap *map=ServersideAttributeMap_new(h);CHECK(shared&&map);
    IAttributeInstance *i=BaseAttributeMap_registerAttribute(&map->base,shared->maxHealth);CHECK(i);CHECK(IAttributeInstance_applyModifier(i,modifier(h,6,"clone",2,0)));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)map));MCObjectRootScope_end(&s);
    CHECK(MCObjectHeap_collect(h));MCObjectHeap *working=MCObjectHeap_clone(h);CHECK(working);
    MCObjectRoot workingRoot={0};CHECK(MCObjectRoot_rebind(&workingRoot,working,&root));
    ServersideAttributeMap *copy=(ServersideAttributeMap *)MCObjectRoot_get(&workingRoot);CHECK(copy);
    SharedMonsterAttributes *copiedShared=SharedMonsterAttributes_get(working);CHECK(copiedShared&&copiedShared!=shared);
    IAttributeInstance *copied=BaseAttributeMap_getAttributeInstance(&copy->base,copiedShared->maxHealth);CHECK(copied&&copied!=i);
    CHECK(IAttributeInstance_getAttributeValue(copied)==22);CHECK(IAttributeInstance_setBaseValue(copied,30));CHECK(IAttributeInstance_getAttributeValue(copied)==32);CHECK(((ModifiableAttributeInstance *)i)->baseValue==20);
    CHECK(MCObjectHeap_adopt(h,working));CHECK(MCObjectRoot_rebind(&root,h,&workingRoot));MCObjectHeap_free(working);
    map=(ServersideAttributeMap *)MCObjectRoot_get(&root);shared=SharedMonsterAttributes_get(h);CHECK(map&&shared);CHECK(IAttributeInstance_getAttributeValue(BaseAttributeMap_getAttributeInstance(&map->base,shared->maxHealth))==32);
    CHECK(MCObjectHeap_collect(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
    h=MCObjectHeap_new(64);CHECK(h);CHECK(SharedMonsterAttributes_get(h)==NULL&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
int main(void) {
    source_values_and_live_views();inherited_modifier_identity_and_cache();nbt_saved_flags_and_duplicate_replacement();bulk_entries_and_collection_resource_boundary();failures_and_lifetime();
    printf("Source attributes: %u checks\n",checks);return 0;
}
