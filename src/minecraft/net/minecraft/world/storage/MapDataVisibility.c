#include "world/storage/MapDataVisibility.h"
#include "item/ItemStackFrame.h"
#include "entity/player/EntityPlayer.h"
#include "util/MCGameplayPlayer.h"
#include "nbt/NBTInternal.h"
#include "world/World.h"
#include <limits.h>
#include <stdio.h>

typedef struct VisibilityCall {
    MapData *map;
    MCObjectHeap *heap;
    MCObjectRootScope scope;
    const MapDataVisibilityDependencies *deps;
    MCObject *context;
} VisibilityCall;
static WorldSavedDataResult fail(VisibilityCall *v) {
    MCObjectHeap_fail(v->heap);
    return WORLD_SAVED_DATA_FAILURE;
}
static bool identity(const MCObject *o,void *c) { return o==c; }
static bool tracked(VisibilityCall *v,const MCObject *o) {
    return !o || (o->heap==v->heap &&
        MCObjectHeap_findObject(v->heap,o->klass,identity,(void *)o)==o);
}
static WorldSavedDataResult ref(VisibilityCall *v,MCObject *o) {
    return tracked(v,o) && (!o || MCObjectRootScope_pin(&v->scope,o))
        ? WORLD_SAVED_DATA_OK : fail(v);
}
static WorldSavedDataResult result(VisibilityCall *v,WorldSavedDataResult r) {
    if(!tracked(v,(MCObject *)v->map) || !MapData_isInstance((MCObject *)v->map) ||
       !tracked(v,v->context) || !tracked(v,v->map?v->map->base.nativeContext:NULL) || MCObjectHeap_failed(v->heap) ||
       (r!=WORLD_SAVED_DATA_OK && r!=WORLD_SAVED_DATA_EXCEPTION)) return fail(v);
    return r;
}
static WorldSavedDataResult required(VisibilityCall *v,MCObject *o,bool (*valid)(const MCObject *)) {
    if(!o)return WORLD_SAVED_DATA_EXCEPTION;
    if(!tracked(v,o) || !valid(o))return fail(v);
    return ref(v,o);
}
static WorldSavedDataResult sized(VisibilityCall *v,MCObject *o,bool (*valid)(const MCObject *),size_t bytes) {
    if(!o)return WORLD_SAVED_DATA_EXCEPTION;
    if(!tracked(v,o) || MCObjectHeap_objectSize(o)<bytes)return fail(v);
    return required(v,o,valid);
}
static WorldSavedDataResult player(VisibilityCall *v,MCGameplayPlayer *p) {
    return required(v,(MCObject *)p,MCGameplayPlayer_isInstance);
}
static WorldSavedDataResult stack(VisibilityCall *v,ItemStack *s) {
    return sized(v,(MCObject *)s,ItemStack_isInstance,sizeof(ItemStack));
}
static WorldSavedDataResult compound(VisibilityCall *v,NBTTagCompound *t) {
    return required(v,(MCObject *)t,NBTTagCompound_isInstance);
}
#define TRY(expression) do { r=result(v,(expression)); if(r!=WORLD_SAVED_DATA_OK)goto done; } while(0)
#define LEAF(expression) do { WorldSavedDataResult z=result(v,(expression)); if(z!=WORLD_SAVED_DATA_OK)return z; } while(0)

static WorldSavedDataResult inventory_has(VisibilityCall *v,InventoryPlayer *p,ItemStack *s,bool *out) {
    LEAF(sized(v,(MCObject *)p,InventoryPlayer_isInstance,sizeof(InventoryPlayer)));
    if(v->deps && v->deps->inventoryHasItemStack)
        return result(v,v->deps->inventoryHasItemStack(v->context,p,s,out));
    /* Validate only arrays/entries the concrete original method reaches. A
       matching armor entry must not inspect a later nullable main array. */
    for(int group=0;group<2;group++) {
        ItemStackArray *a=group?p->mainInventory:p->armorInventory;
        LEAF(sized(v,(MCObject *)a,ItemStackArray_isInstance,sizeof(ItemStackArray)));
        if(a->length<0 || (size_t)a->length>(MCObjectHeap_objectSize((MCObject *)a)-sizeof(*a))/sizeof(*a->items))return fail(v);
        for(int32_t i=0;i<a->length;i++) if(a->items[i]) {
            LEAF(stack(v,a->items[i]));
            if(s)LEAF(stack(v,s));
            if(ItemStack_isItemEqual(a->items[i],s)) {
                *out=InventoryPlayer_hasItemStack(p,s);
                return result(v,WORLD_SAVED_DATA_OK);
            }
        }
    }
    *out=InventoryPlayer_hasItemStack(p,s);
    return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult name(VisibilityCall *v,MCGameplayPlayer *p,NBTString **out) {
    LEAF(player(v,p));
    NBTString *s=NULL;
    if(v->deps && v->deps->playerGetName)
        LEAF(v->deps->playerGetName(v->context,p,&s));
    else {
        if(!p->gameProfile)return WORLD_SAVED_DATA_EXCEPTION;
        s=EntityPlayer_getName(p);
        LEAF(WORLD_SAVED_DATA_OK);
    }
    LEAF(ref(v,(MCObject *)s));
    if(s && !NBTString_isInstance((MCObject *)s))return fail(v);
    *out=s;return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult on_frame(VisibilityCall *v,ItemStack *s,bool *out) {
    LEAF(stack(v,s));
    *out=ItemStack_isOnItemFrame(s);return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult frame(VisibilityCall *v,ItemStack *s,EntityItemFrame **out) {
    LEAF(stack(v,s));EntityItemFrame *f=NULL;
    f=ItemStack_getItemFrame(s);
    LEAF(ref(v,(MCObject *)f));
    if(f && !EntityItemFrame_isInstance((MCObject *)f))return fail(v);
    *out=f;return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult frame_position(VisibilityCall *v,EntityItemFrame *f,BlockPos **out) {
    LEAF(required(v,(MCObject *)f,EntityItemFrame_isInstance));BlockPos *p=NULL;
    if(v->deps && v->deps->frameGetHangingPosition)LEAF(v->deps->frameGetHangingPosition(v->context,f,&p));
    else p=EntityHanging_getHangingPosition(&f->hanging);
    LEAF(ref(v,(MCObject *)p));
    if(p && !BlockPos_isInstance((MCObject *)p))return fail(v);
    *out=p;return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult frame_id(VisibilityCall *v,EntityItemFrame *f,int32_t *out) {
    LEAF(required(v,(MCObject *)f,EntityItemFrame_isInstance));
    if(v->deps && v->deps->frameGetEntityId)
        return result(v,v->deps->frameGetEntityId(v->context,f,out));
    *out=Entity_getEntityId(&f->hanging.entity);return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult position_coord(VisibilityCall *v,BlockPos *p,bool x,int32_t *out) {
    LEAF(required(v,(MCObject *)p,BlockPos_isInstance));
    if(v->deps && x && v->deps->positionGetX)return result(v,v->deps->positionGetX(v->context,p,out));
    if(v->deps && !x && v->deps->positionGetZ)return result(v,v->deps->positionGetZ(v->context,p,out));
    *out=x?p->x:p->z;return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult facing_index(VisibilityCall *v,const NativeHangingFacing *f,int32_t *out) {
    if(!f)return WORLD_SAVED_DATA_EXCEPTION;
    if(v->deps && v->deps->facingGetHorizontalIndex)
        return result(v,v->deps->facingGetHorizontalIndex(v->context,f,out));
    return NativeHangingFacing_getHorizontalIndex(f,out)?WORLD_SAVED_DATA_OK:fail(v);
}
static WorldSavedDataResult has_tag(VisibilityCall *v,ItemStack *s,bool *out) {
    LEAF(stack(v,s));
    *out=ItemStack_hasTagCompound(s);return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult get_tag(VisibilityCall *v,ItemStack *s,NBTTagCompound **out) {
    LEAF(stack(v,s));NBTTagCompound *t=NULL;
    t=ItemStack_getTagCompound(s);
    LEAF(ref(v,(MCObject *)t));
    if(t && !NBTTagCompound_isInstance((MCObject *)t))return fail(v);
    *out=t;return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult has_key(VisibilityCall *v,NBTTagCompound *t,bool *out) {
    LEAF(compound(v,t));
    if(v->deps && v->deps->tagHasKeyType)
        return result(v,v->deps->tagHasKeyType(v->context,t,"Decorations",9,out));
    *out=NBTTagCompound_hasKeyType_ascii(t,"Decorations",9);return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult tag_list(VisibilityCall *v,NBTTagCompound *t,NBTTagList **out) {
    LEAF(compound(v,t));NBTTagList *l=NULL;
    if(v->deps && v->deps->tagGetTagList)LEAF(v->deps->tagGetTagList(v->context,t,"Decorations",10,&l));
    else l=NBTTagCompound_getTagList_ascii(t,"Decorations",10);
    LEAF(ref(v,(MCObject *)l));
    if(l && !NBTTagList_isInstance((MCObject *)l))return fail(v);
    *out=l;return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult count(VisibilityCall *v,NBTTagList *l,int32_t *out) {
    LEAF(required(v,(MCObject *)l,NBTTagList_isInstance));
    if(v->deps && v->deps->tagCount)return result(v,v->deps->tagCount(v->context,l,out));
    *out=NBTTagList_tagCount(l);return WORLD_SAVED_DATA_OK;
}
static WorldSavedDataResult entry(VisibilityCall *v,NBTTagList *l,int32_t index,NBTTagCompound **out) {
    LEAF(required(v,(MCObject *)l,NBTTagList_isInstance));NBTTagCompound *t=NULL;
    if(v->deps && v->deps->tagGetCompoundAt)LEAF(v->deps->tagGetCompoundAt(v->context,l,index,&t));
    else t=NBTTagList_getCompoundTagAt(l,index);
    LEAF(ref(v,(MCObject *)t));
    if(t && !NBTTagCompound_isInstance((MCObject *)t))return fail(v);
    *out=t;return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult tag_string(VisibilityCall *v,NBTTagCompound *t,NBTString **out) {
    LEAF(compound(v,t));NBTString *s=NULL;
    if(v->deps && v->deps->tagGetString)LEAF(v->deps->tagGetString(v->context,t,"id",&s));
    else s=NBTTagCompound_getString_ascii(t,"id");
    LEAF(ref(v,(MCObject *)s));
    if(s && !NBTString_isInstance((MCObject *)s))return fail(v);
    *out=s;return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult tag_byte(VisibilityCall *v,NBTTagCompound *t,int32_t *out) {
    LEAF(compound(v,t));
    if(v->deps && v->deps->tagGetByte)
        return result(v,v->deps->tagGetByte(v->context,t,"type",out));
    *out=NBTTagCompound_getByte_ascii(t,"type");return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult tag_double(VisibilityCall *v,NBTTagCompound *t,const char *key,double *out) {
    LEAF(compound(v,t));
    if(v->deps && v->deps->tagGetDouble)
        return result(v,v->deps->tagGetDouble(v->context,t,key,out));
    *out=NBTTagCompound_getDouble_ascii(t,key);return result(v,WORLD_SAVED_DATA_OK);
}
static WorldSavedDataResult array_result(VisibilityCall *v,NativeArrayResult r) {
    return result(v,r==NATIVE_ARRAY_OK?WORLD_SAVED_DATA_OK:
        r==NATIVE_ARRAY_EXCEPTION?WORLD_SAVED_DATA_EXCEPTION:WORLD_SAVED_DATA_FAILURE);
}
static WorldSavedDataResult list_size(VisibilityCall *v,NativeReferenceList *l,int32_t *out) {
    if(!l)return WORLD_SAVED_DATA_EXCEPTION;
    LEAF(required(v,(MCObject *)l,NativeReferenceList_isInstance));
    if(v->deps && v->deps->listSize)return result(v,v->deps->listSize(v->context,l,out));
    return array_result(v,NativeReferenceList_sizeSource(l,out));
}
static WorldSavedDataResult list_get(VisibilityCall *v,NativeReferenceList *l,int32_t index,MCObject **out) {
    if(!l)return WORLD_SAVED_DATA_EXCEPTION;
    LEAF(required(v,(MCObject *)l,NativeReferenceList_isInstance));MCObject *o=NULL;
    if(v->deps && v->deps->listGet)LEAF(v->deps->listGet(v->context,l,index,&o));
    else LEAF(array_result(v,NativeReferenceList_getSource(l,index,&o)));
    LEAF(ref(v,o));*out=o;return WORLD_SAVED_DATA_OK;
}
static NativeArrayResult info_equals(MCObject *c,MCObject *query,MCObject *stored,bool *out) {
    (void)c;*out=query==stored;return NATIVE_ARRAY_OK;
}
static const NativeReferenceListEqualsMethods infoEquals={info_equals};
static WorldSavedDataResult decorate(VisibilityCall *v,int32_t type,World *w,NBTString *id,double x,double z,double rot) {
    LEAF(ref(v,(MCObject *)w));
    if(w && !World_isInstance((MCObject *)w))return fail(v);
    return result(v,MapData_updateDecorations(v->map,type,w,id,x,z,rot));
}
static int32_t signed32(uint32_t x) { return x<=INT32_MAX?(int32_t)x:-1-(int32_t)(UINT32_MAX-x); }

WorldSavedDataResult MapData_updateVisiblePlayersWithDependencies(MapData *m,MCGameplayPlayer *p,
    ItemStack *s,const MapDataVisibilityDependencies *d,MCObject *c) {
    VisibilityCall call={0},*v=&call;
    v->map=m;v->heap=m?((MCObject *)m)->heap:NULL;v->deps=d;v->context=c;
    if(!MapData_isInstance((MCObject *)m) || MCObjectHeap_failed(v->heap) ||
       !MCObjectRootScope_begin(&v->scope,v->heap))return fail(v);
    WorldSavedDataResult r=WORLD_SAVED_DATA_OK;
    TRY(ref(v,(MCObject *)m));TRY(ref(v,(MCObject *)p));TRY(ref(v,(MCObject *)s));TRY(ref(v,c));

    NativeHashMap *hash=m->playersHashMap;
    if(!hash){r=WORLD_SAVED_DATA_EXCEPTION;goto done;}
    TRY(required(v,(MCObject *)hash,NativeHashMap_isInstance));
    bool present=NativeHashMap_containsKey(hash,(MCObject *)p);
    TRY(WORLD_SAVED_DATA_OK);
    if(!present) {
        MapInfo *i=MapInfo_new(v->heap,m,p,NULL,NULL);
        TRY(ref(v,(MCObject *)i));if(!i){r=fail(v);goto done;}
        hash=m->playersHashMap;
        if(!hash){r=WORLD_SAVED_DATA_EXCEPTION;goto done;}
        TRY(required(v,(MCObject *)hash,NativeHashMap_isInstance));
        if(!NativeHashMap_put(hash,(MCObject *)p,(MCObject *)i)){r=fail(v);goto done;}
        NativeReferenceList *l=m->playersArrayList;
        if(!l){r=WORLD_SAVED_DATA_EXCEPTION;goto done;}
        TRY(required(v,(MCObject *)l,NativeReferenceList_isInstance));
        if(!NativeReferenceList_add(l,(MCObject *)i)){r=fail(v);goto done;}
    }

    bool has=false,on=false;
    TRY(player(v,p));TRY(inventory_has(v,p->inventory,s,&has));
    if(!has) {
        NativeLinkedHashMap *decorations=m->mapDecorations;
        TRY(ref(v,(MCObject *)decorations));
        NBTString *id=NULL;TRY(name(v,p,&id));
        if(!decorations){r=WORLD_SAVED_DATA_EXCEPTION;goto done;}
        TRY(required(v,(MCObject *)decorations,NativeLinkedHashMap_isInstance));
        NativeLinkedHashMap_remove(decorations,(MCObject *)id);TRY(WORLD_SAVED_DATA_OK);
    }
    for(int32_t index=0;;index=signed32((uint32_t)index+1u)) {
        int32_t n=0;TRY(list_size(v,m->playersArrayList,&n));if(index>=n)break;
        MCObject *value=NULL;TRY(list_get(v,m->playersArrayList,index,&value));
        if(value && !MapInfo_isInstance(value)) {
            r=MapInfo_isRuntimeClass(value)?fail(v):WORLD_SAVED_DATA_EXCEPTION;goto done;
        }
        MapInfo *i=(MapInfo *)value;
        if(!i){r=WORLD_SAVED_DATA_EXCEPTION;goto done;}
        MCGameplayPlayer *viewer=i->entityplayerObj;TRY(player(v,viewer));
        bool retained=!viewer->living.entity.isDead;
        if(retained) {
            viewer=i->entityplayerObj;TRY(player(v,viewer));
            TRY(inventory_has(v,viewer->inventory,s,&has));
            if(!has){TRY(on_frame(v,s,&on));retained=on;}
        }
        if(retained) {
            TRY(on_frame(v,s,&on));
            if(!on) {
                viewer=i->entityplayerObj;TRY(player(v,viewer));
                if(viewer->living.entity.dimension==m->dimension) {
                    viewer=i->entityplayerObj;TRY(player(v,viewer));
                    World *w=(World *)viewer->living.entity.worldObj;TRY(ref(v,(MCObject *)w));
                    viewer=i->entityplayerObj;NBTString *id=NULL;TRY(name(v,viewer,&id));
                    viewer=i->entityplayerObj;TRY(player(v,viewer));double x=viewer->living.entity.posX;
                    viewer=i->entityplayerObj;TRY(player(v,viewer));double z=viewer->living.entity.posZ;
                    viewer=i->entityplayerObj;TRY(player(v,viewer));double yaw=(double)viewer->living.entity.rotationYaw;
                    TRY(decorate(v,0,w,id,x,z,yaw));
                }
            }
        } else {
            hash=m->playersHashMap;TRY(ref(v,(MCObject *)hash));
            viewer=i->entityplayerObj;TRY(ref(v,(MCObject *)viewer));
            if(!hash){r=WORLD_SAVED_DATA_EXCEPTION;goto done;}
            TRY(required(v,(MCObject *)hash,NativeHashMap_isInstance));
            NativeHashMap_remove(hash,(MCObject *)viewer);TRY(WORLD_SAVED_DATA_OK);
            NativeReferenceList *l=m->playersArrayList;
            if(!l){r=WORLD_SAVED_DATA_EXCEPTION;goto done;}
            bool removed=false;
            TRY(array_result(v,NativeReferenceList_removeObjectSource(l,(MCObject *)i,&infoEquals,NULL,&removed)));
        }
    }

    TRY(on_frame(v,s,&on));
    if(on) {
        EntityItemFrame *f=NULL;BlockPos *pos=NULL;
        TRY(frame(v,s,&f));TRY(frame_position(v,f,&pos));
        TRY(player(v,p));World *w=(World *)p->living.entity.worldObj;TRY(ref(v,(MCObject *)w));
        int32_t eid=0,x=0,z=0,horizontal=0;TRY(frame_id(v,f,&eid));
        char text[32];snprintf(text,sizeof(text),"frame-%d",(int)eid);
        NBTString *id=NBTString_fromASCII(v->heap,text);TRY(ref(v,(MCObject *)id));
        if(!id){r=fail(v);goto done;}
        TRY(position_coord(v,pos,true,&x));TRY(position_coord(v,pos,false,&z));
        TRY(facing_index(v,f->hanging.facingDirection,&horizontal));
        TRY(decorate(v,1,w,id,(double)x,(double)z,(double)signed32((uint32_t)horizontal*90u)));
    }
    TRY(has_tag(v,s,&has));
    if(has) {
        NBTTagCompound *tag=NULL;TRY(get_tag(v,s,&tag));TRY(has_key(v,tag,&has));
        if(has) {
            TRY(get_tag(v,s,&tag));NBTTagList *l=NULL;TRY(tag_list(v,tag,&l));
            for(int32_t j=0;;j=signed32((uint32_t)j+1u)) {
                int32_t n=0;TRY(count(v,l,&n));if(j>=n)break;
                NBTTagCompound *t=NULL;TRY(entry(v,l,j,&t));
                NativeLinkedHashMap *decorations=m->mapDecorations;TRY(ref(v,(MCObject *)decorations));
                NBTString *id=NULL;TRY(tag_string(v,t,&id));
                if(!decorations){r=WORLD_SAVED_DATA_EXCEPTION;goto done;}
                TRY(required(v,(MCObject *)decorations,NativeLinkedHashMap_isInstance));
                present=NativeLinkedHashMap_containsKey(decorations,(MCObject *)id);TRY(WORLD_SAVED_DATA_OK);
                if(!present) {
                    int32_t type=0;TRY(tag_byte(v,t,&type));
                    TRY(player(v,p));World *w=(World *)p->living.entity.worldObj;TRY(ref(v,(MCObject *)w));
                    TRY(tag_string(v,t,&id));
                    double x=0,z=0,rot=0;TRY(tag_double(v,t,"x",&x));TRY(tag_double(v,t,"z",&z));TRY(tag_double(v,t,"rot",&rot));
                    TRY(decorate(v,type,w,id,x,z,rot));
                }
            }
        }
    }
done:
    r=result(v,r);MCObjectRootScope_end(&v->scope);return r;
}
WorldSavedDataResult MapData_updateVisiblePlayers(MapData *m,MCGameplayPlayer *p,ItemStack *s) {
    return MapData_updateVisiblePlayersWithDependencies(m,p,s,NULL,NULL);
}
