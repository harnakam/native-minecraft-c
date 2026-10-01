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
    f->entity->entity.posX=1.25;f->entity->entity.posY=10;f->entity->entity.posZ=-2.5;
    f->entity->entity.motionX=.2;f->entity->entity.motionY=-.1;f->entity->entity.motionZ=.3;
    f->entity->entity.width=.25f;f->entity->entity.height=.25f;
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
            double *fields[]={&f.entity->entity.posX,&f.entity->entity.posY,&f.entity->entity.posZ,
                              &f.entity->entity.motionX,&f.entity->entity.motionY,&f.entity->entity.motionZ};
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
            if (field)f.entity->entity.height=sizes[n];else f.entity->entity.width=sizes[n];
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
    f.entity->entity.posX=limit;f.entity->entity.posY=limit;f.entity->entity.posZ=-limit;
    f.entity->entity.motionX=-16;f.entity->entity.motionY=-16;f.entity->entity.motionZ=16;
    f.entity->entity.width=8;f.entity->entity.height=8;
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
    CHECK(f.entity->entity.ticksExisted==1&&f.entity->age==1&&f.entity->delayBeforeCanPickup==39);
    CHECK(f.entity->entity.posX>1.25&&f.entity->entity.posY<10&&f.entity->entity.posZ>-2.5);
    CHECK(EntityItem_getEntityItem(f.entity)==f.stack&&f.stack->stackSize==0);
    CHECK(f.stack->stackTagCompound==tag&&NBTTagCompound_getInteger_ascii(tag,"foreign")==19);
    f.entity->age=6000;
    unsigned char before[sizeof(EntityItem)];memcpy(before,f.entity,sizeof before);
    CHECK(!NativeItemMotion_tick(f.entity,&f.terrain));
    CHECK(!MCObjectHeap_failed(f.heap)&&!memcmp(before,f.entity,sizeof before));
    end(&f);
}

static bool source_init(MCObject *c,Entity *e) {(void)c;return EntityItem_entityInit((EntityItem *)e);}
static bool source_position(MCObject *c,Entity *e,double x,double y,double z) {(void)c;return Entity_setPosition(e,x,y,z);}
static bool source_bounds(MCObject *c,Entity *e,AxisAlignedBB *b) {(void)c;return Entity_setEntityBoundingBox(e,b);}
static const EntityDependencies source_methods={.entityInit=source_init,.setPosition=source_position,.setEntityBoundingBox=source_bounds};
static bool source_base(MCObject *c,EntityItem *e,MCObject *w) {
    NativeEntityIDRuntime *ids=NativeEntityIDRuntime_new(7);CHECK(ids);
    bool ok=Entity_construct(&e->entity,w,&source_methods,c,NativeJavaRandomRuntime_process(),ids);
    CHECK(NativeEntityIDRuntime_free(ids));return ok;
}
static double source_math(MCObject *c) {(void)c;return .25;}
static bool source_size(MCObject *c,EntityItem *e,float w,float h) {(void)c;return Entity_setSize(&e->entity,w,h);}
static bool source_item_position(MCObject *c,EntityItem *e,double x,double y,double z) {(void)c;return Entity_setPosition(&e->entity,x,y,z);}
static const EntityItemConstructorDependencies source_constructors={source_base,source_math,source_size,source_item_position};
static void source_owned_box_survives_motion(void) {
    Fixture f;begin(&f);
    EntityItem *e=EntityItem_new_world(f.heap,NULL,NULL,mc_server_graph_item_dependencies(),&source_constructors);
    CHECK(e&&e->entity.entityDependencies==&source_methods&&e->entity.boundingBox);
    AxisAlignedBB *initial=e->entity.boundingBox;
    double old_min=-(double)(.6f/2.0f);
    CHECK(initial->minX==old_min&&initial->maxX==old_min+.25&&e->entity.posX==0);
    CHECK(EntityItem_setEntityItemStack(e,f.stack));
    NBTTagCompound *tag=f.stack->stackTagCompound;
    CHECK(NativeItemMotion_tick(e,&f.terrain));
    CHECK(!MCObjectHeap_failed(f.heap)&&e->entity.boundingBox!=initial);
    CHECK(e->entity.posX==old_min+.125&&e->entity.posZ==old_min+.125);
    CHECK(e->entity.boundingBox->minX==old_min&&e->entity.boundingBox->minZ==old_min);
    CHECK(e->entity.boundingBox->minY==e->entity.posY&&e->entity.boundingBox->maxY==e->entity.posY+.25);
    CHECK(EntityItem_getEntityItem(e)==f.stack&&f.stack->stackSize==0&&f.stack->stackTagCompound==tag);
    end(&f);
}
int main(void) {
    source_owned_box_survives_motion();
    invalid_scalars_fail_without_mutation();
    native_envelope_boundaries();
    valid_motion_preserves_nonnull_zero_stack_and_tag_alias();
    printf("native item motion: %u checks passed\n",checks);
    return 0;
}
