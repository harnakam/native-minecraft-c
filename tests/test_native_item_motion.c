#include "entity/item/NativeItemMotion.h"
#include "server/native_gameplay.h"
#include "nbt/NBTTagCompound.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
    fprintf(stderr,"native item motion check %u at %d: %s\n",checks,__LINE__,#expression); \
    exit(1); } } while (0)

typedef struct {
    MCObjectHeap *heap;
    MCObjectRootScope scope;
    EntityItem *entity;
    ItemStack *stack;
    mc_world terrain;
} Fixture;

static void begin(Fixture *f) {
    memset(f,0,sizeof(*f));
    f->heap=MCObjectHeap_new(8u*1024u*1024u);
    CHECK(f->heap&&MCObjectRootScope_begin(&f->scope,f->heap));
    /* Explicit native allocation with the real runtime's immutable dependency
       table. No world/effect callback is invoked by this collision kernel. */
    f->entity=EntityItem_nativeNew(f->heap,NULL,NULL,mc_server_graph_item_dependencies());
    CHECK(f->entity&&EntityItem_nativeInitializeDataWatcher(f->entity,NULL,NULL));
    f->stack=ItemStack_new(f->heap,ItemStack_registryItem(264),0,7);
    CHECK(f->stack&&EntityItem_setEntityItemStack(f->entity,f->stack));
    NBTTagCompound *tag=NBTTagCompound_new(f->heap);
    CHECK(tag&&NBTTagCompound_setInteger_ascii(tag,"foreign",19)&&ItemStack_setTagCompound(f->stack,tag));
    f->entity->posX=1.25;f->entity->posY=10;f->entity->posZ=-2.5;
    f->entity->motionX=.2;f->entity->motionY=-.1;f->entity->motionZ=.3;
    f->entity->width=.25f;f->entity->height=.25f;
    f->entity->delayBeforeCanPickup=40;
    mc_world_init(&f->terrain,919);
}

static void end(Fixture *f) {
    mc_world_free(&f->terrain);
    MCObjectRootScope_end(&f->scope);
    MCObjectHeap_free(f->heap);
}

static void invalid_scalars_fail_without_mutation(void) {
    const double values[]={1e20,-1e20,INFINITY,-INFINITY,NAN};
    for (unsigned field=0;field<6;field++) {
        for (unsigned n=0;n<sizeof(values)/sizeof(*values);n++) {
            Fixture f;begin(&f);
            double *fields[]={&f.entity->posX,&f.entity->posY,&f.entity->posZ,
                              &f.entity->motionX,&f.entity->motionY,&f.entity->motionZ};
            *fields[field]=values[n];
            unsigned char before[sizeof(EntityItem)];memcpy(before,f.entity,sizeof before);
            CHECK(!NativeItemMotion_tick(f.entity,&f.terrain));
            CHECK(MCObjectHeap_failed(f.heap));
            CHECK(!memcmp(before,f.entity,sizeof before));
            CHECK(f.stack->stackSize==0&&f.stack->itemDamage==7&&f.stack->stackTagCompound);
            end(&f);
        }
    }
    const float sizes[]={0,-1,9,INFINITY,NAN};
    for (unsigned field=0;field<2;field++) {
        for (unsigned n=0;n<sizeof(sizes)/sizeof(*sizes);n++) {
            Fixture f;begin(&f);
            if (field)f.entity->height=sizes[n];else f.entity->width=sizes[n];
            unsigned char before[sizeof(EntityItem)];memcpy(before,f.entity,sizeof before);
            CHECK(!NativeItemMotion_tick(f.entity,&f.terrain)&&MCObjectHeap_failed(f.heap));
            CHECK(!memcmp(before,f.entity,sizeof before));
            end(&f);
        }
    }
}

static void native_envelope_boundaries(void) {
    const double limit=67108832.0;
    CHECK(NativeItemMotion_positionSupported(limit,-limit,limit));
    CHECK(!NativeItemMotion_positionSupported(nextafter(limit,INFINITY),0,0));
    CHECK(!NativeItemMotion_positionSupported(0,nextafter(-limit,-INFINITY),0));
    CHECK(!NativeItemMotion_positionSupported(0,0,1e20));
    CHECK(!NativeItemMotion_positionSupported(NAN,0,0));
    Fixture f;begin(&f);
    f.entity->posX=limit;f.entity->posY=limit;f.entity->posZ=-limit;
    f.entity->motionX=-16;f.entity->motionY=-16;f.entity->motionZ=16;
    f.entity->width=8;f.entity->height=8;
    CHECK(NativeItemMotion_validate(f.entity));
    CHECK(NativeItemMotion_tick(f.entity,&f.terrain));
    CHECK(!MCObjectHeap_failed(f.heap));
    CHECK(EntityItem_getEntityItem(f.entity)==f.stack&&f.stack->stackSize==0);
    end(&f);
}

static void valid_motion_preserves_nonnull_zero_stack_and_tag_alias(void) {
    Fixture f;begin(&f);
    NBTTagCompound *tag=f.stack->stackTagCompound;
    CHECK(EntityItem_getEntityItem(f.entity)==f.stack&&f.stack->stackSize==0);
    CHECK(NativeItemMotion_tick(f.entity,&f.terrain));
    CHECK(!MCObjectHeap_failed(f.heap));
    CHECK(f.entity->ticksExisted==1&&f.entity->age==1&&f.entity->delayBeforeCanPickup==39);
    CHECK(f.entity->posX>1.25&&f.entity->posY<10&&f.entity->posZ>-2.5);
    CHECK(EntityItem_getEntityItem(f.entity)==f.stack&&f.stack->stackSize==0);
    CHECK(f.stack->stackTagCompound==tag&&NBTTagCompound_getInteger_ascii(tag,"foreign")==19);
    f.entity->age=6000;
    unsigned char before[sizeof(EntityItem)];memcpy(before,f.entity,sizeof before);
    CHECK(!NativeItemMotion_tick(f.entity,&f.terrain));
    CHECK(!MCObjectHeap_failed(f.heap)&&!memcmp(before,f.entity,sizeof before));
    end(&f);
}

int main(void) {
    invalid_scalars_fail_without_mutation();
    native_envelope_boundaries();
    valid_motion_preserves_nonnull_zero_stack_and_tag_alias();
    printf("native item motion: %u checks passed\n",checks);
    return 0;
}
