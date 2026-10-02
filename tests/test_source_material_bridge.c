#include "util/MCGameplay.h"
#include "util/MCGameplayWorld.h"
#include "util/NativeBlockStateRuntime.h"
#include "server/native_gameplay.h"
#include "entity/player/EntityPlayerMP.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { \
    fprintf(stderr,"material bridge check %u at %d: %s\n",checks,__LINE__,#x); \
    exit(1); } } while (0)

/* Source Block.getMaterial retains one reference. The native dense boundary
   must select that same owner instead of allocating a property snapshot. */
static void dense_material_repeated_alias(void) {
    mc_world *terrain=calloc(1,sizeof(*terrain));
    CHECK(terrain);
    mc_world_init(terrain,7);
    CHECK(mc_world_set(terrain,1,2,3,1u<<4));
    MCGameplay game={0};
    CHECK(MCGameplay_init(&game,32u*1024u*1024u));
    MCObjectRootScope scope={0};
    CHECK(MCObjectRootScope_begin(&scope,game.heap));
    MCGameplayWorld *world=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),terrain,NULL);
    CHECK(world);
    BlockPos *pos=BlockPos_newInt(game.heap,1,2,3);
    CHECK(pos);
    MCObject *chunk=world->dependencies->getChunkFromBlockCoords(world->dependencyContext,world,pos);
    CHECK(chunk);
    MCObject *block=world->dependencies->chunkGetBlock(world->dependencyContext,chunk,pos);
    CHECK(block);
    CHECK(Material_getStatics(game.heap));
    size_t beforeObjects=MCObjectHeap_liveObjects(game.heap),beforeBytes=MCObjectHeap_liveBytes(game.heap);
    MCObject *first=world->dependencies->blockGetMaterial(world->dependencyContext,block);
    MCObject *second=world->dependencies->blockGetMaterial(world->dependencyContext,block);
    CHECK(first && second);
    CHECK(first==second);
    CHECK(MCObjectHeap_liveObjects(game.heap)==beforeObjects &&
        MCObjectHeap_liveBytes(game.heap)==beforeBytes);
    CHECK(!MCObjectHeap_failed(game.heap));
    MCObjectRootScope_end(&scope);
    CHECK(MCGameplay_free(&game));
    mc_world_free(terrain);
    free(terrain);
}

static void expected_materials(MaterialStatics *s, Material **out) {
    /* Independently resolved supplied registry constructors, not registryFacts. */
    Material *values[198] = {
        s->air, s->rock, s->grass, s->ground, s->rock, s->wood, s->plants, s->rock,
        s->water, s->water, s->lava, s->lava, s->sand, s->sand, s->rock, s->rock,
        s->rock, s->wood, s->leaves, s->sponge, s->glass, s->rock, s->iron, s->rock,
        s->rock, s->wood, s->cloth, s->circuits, s->circuits, s->piston, s->web, s->vine,
        s->vine, s->piston, s->piston, s->cloth, s->piston, s->plants, s->plants, s->plants,
        s->plants, s->iron, s->iron, s->rock, s->rock, s->rock, s->tnt, s->wood,
        s->rock, s->rock, s->circuits, s->fire, s->rock, s->wood, s->wood, s->circuits,
        s->rock, s->iron, s->wood, s->plants, s->ground, s->rock, s->rock, s->wood,
        s->wood, s->circuits, s->circuits, s->rock, s->wood, s->circuits, s->rock, s->iron,
        s->wood, s->rock, s->rock, s->circuits, s->circuits, s->circuits, s->snow, s->ice,
        s->craftedSnow, s->cactus, s->clay, s->plants, s->wood, s->wood, s->gourd, s->rock,
        s->sand, s->glass, s->portal, s->gourd, s->cake, s->circuits, s->circuits, s->glass,
        s->wood, s->clay, s->rock, s->wood, s->wood, s->iron, s->glass, s->gourd,
        s->plants, s->plants, s->vine, s->wood, s->rock, s->rock, s->grass, s->plants,
        s->rock, s->rock, s->rock, s->plants, s->rock, s->iron, s->iron, s->portal,
        s->rock, s->rock, s->dragonEgg, s->redstoneLight, s->redstoneLight, s->wood, s->wood, s->plants,
        s->rock, s->rock, s->rock, s->circuits, s->circuits, s->iron, s->wood, s->wood,
        s->wood, s->iron, s->glass, s->rock, s->circuits, s->plants, s->plants, s->circuits,
        s->circuits, s->anvil, s->wood, s->iron, s->iron, s->circuits, s->circuits, s->wood,
        s->iron, s->rock, s->iron, s->rock, s->rock, s->circuits, s->rock, s->rock,
        s->glass, s->leaves, s->wood, s->wood, s->wood, s->clay, s->barrier, s->iron,
        s->rock, s->glass, s->grass, s->carpet, s->rock, s->rock, s->packedIce, s->vine,
        s->wood, s->wood, s->wood, s->rock, s->rock, s->rock, s->rock, s->wood,
        s->wood, s->wood, s->wood, s->wood, s->wood, s->wood, s->wood, s->wood,
        s->wood, s->wood, s->wood, s->wood, s->wood, s->wood,
    };
    for (int i=0;i<198;i++) out[i]=values[i];
}
static void registry_source_aliases_and_methods(void) {
    MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);
    CHECK(h);
    NativeBlockStateRuntime *r=NativeBlockStateRuntime_get(h);
    MaterialStatics *s=Material_getStatics(h);
    CHECK(r && s);
    Material *expected[198]; expected_materials(s,expected);
    unsigned distinct=0;
    for(int id=0;id<198;id++) {
        NativeBlock *b=NativeBlockStateRuntime_block(r,id);
        CHECK(b && b->registeredId==id && NativeBlock_getMaterial(b)==expected[id]);
        CHECK(Material_isInstance((MCObject *)b->material));
        bool first=true;
        for(int j=0;j<id;j++) if(expected[j]==expected[id])first=false;
        if(first)distinct++;
    }
    CHECK(distinct==34 && r->materials->length==34);
    for(int i=0;i<34;i++) {
        Material *m=(Material *)r->materials->values[i];
        unsigned matches=0;
        for(int j=0;j<198;j++) if(expected[j]==m)matches++;
        CHECK(matches>0);
        for(int j=0;j<i;j++) CHECK(r->materials->values[j]!=(MCObject *)m);
        CHECK(m!=s->coral);
    }
    CHECK(r->airMaterial==s->air && r->leavesMaterial==s->leaves);
    for(int id=0;id<198;id++)CHECK(NativeBlockStateRuntime_materialForBlockId(h,id)==expected[id]);
    CHECK(!NativeBlockStateRuntime_materialForBlockId(h,-1) &&
        !NativeBlockStateRuntime_materialForBlockId(h,198) && !MCObjectHeap_failed(h));
    bool value=true;
    CHECK(Material_getCanBurn(s->rock,&value)==NATIVE_ARRAY_OK && !value);
    Material *returned=NULL;
    CHECK(Material_setBurning(s->rock,&returned)==NATIVE_ARRAY_OK && returned==s->rock);
    CHECK(Material_getCanBurn(NativeBlock_getMaterial(NativeBlockStateRuntime_block(r,1)),&value)==NATIVE_ARRAY_OK && value);
    MapColor *color=NULL;
    CHECK(Material_getMaterialMapColor(NativeBlock_getMaterial(NativeBlockStateRuntime_block(r,170)),&color)==NATIVE_ARRAY_OK && color==MapColor_getStatics(h)->grassColor);
    CHECK(NativeBlockStateRuntime_materialBlocksMovement(r,s->water,&value) && !value);
    CHECK(NativeBlockStateRuntime_materialIsLiquid(r,s->water,&value) && value);
    CHECK(NativeBlockStateRuntime_materialBlocksMovement(r,s->web,&value) && !value);
    CHECK(NativeBlockStateRuntime_materialBlocksMovement(r,s->leaves,&value) && value);
    int states=0,finalIds=0,overwritten=0;
    for(int id=0;id<198;id++)for(int ordinal=0;;ordinal++) {
        NativeBlockState *state=NativeBlockStateRuntime_validState(r,id,ordinal);
        if(!state)break;
        CHECK(state->block==NativeBlockStateRuntime_block(r,id));
        int32_t raw=ObjectIntIdentityMap_get(r->BLOCK_STATE_IDS,(MCObject *)state);
        CHECK(raw==((id<<4)|state->metadata));
        if(NativeBlockStateRuntime_state(r,raw)!=state)overwritten++;
        states++;
    }
    for(int raw=0;raw<65536;raw++) if(NativeBlockStateRuntime_state(r,raw))finalIds++;
    CHECK(states==7806 && overwritten==6412 && finalIds==1394);
    CHECK(!NativeBlockStateRuntime_block(r,-1) && !NativeBlockStateRuntime_block(r,198) && !MCObjectHeap_failed(h));
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)r));
    CHECK(MCObjectHeap_collect(h));
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);
    MCObjectRoot cloneRoot={0};CHECK(MCObjectRoot_rebind(&cloneRoot,copy,&root));
    NativeBlockStateRuntime *cr=(NativeBlockStateRuntime *)MCObjectRoot_get(&cloneRoot);
    MaterialStatics *cs=Material_getStatics(copy);
    CHECK(cr && cs && cr!=r && cs!=s && cr->airMaterial==cs->air && cr->leavesMaterial==cs->leaves);
    CHECK(NativeBlock_getMaterial(NativeBlockStateRuntime_block(cr,1))==cs->rock);
    CHECK(Material_getCanBurn(cs->rock,&value)==NATIVE_ARRAY_OK && value);
    CHECK(MCObjectHeap_collect(copy));
    CHECK(MCObjectHeap_adopt(h,copy));
    r=(NativeBlockStateRuntime *)MCObjectRoot_get(&root);s=Material_getStatics(h);
    CHECK(r && s && r->airMaterial==s->air && r->leavesMaterial==s->leaves);
    CHECK(NativeBlock_getMaterial(NativeBlockStateRuntime_block(r,1))==s->rock);
    CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(copy);MCObjectHeap_free(h);
}
typedef struct {
    MCObject object;
    NativeBlockStateRuntime *runtime;
    MCObject *replacement;
    Material *returned;
    NativeBlock *block;
    unsigned calls;
    bool value,result,replace,corruptBlockRuntime,corruptBlockMaterial;
} HookContext;
static void hook_trace(MCObject *o,MCObjectVisitor v,void *ctx) {
    HookContext *c=(HookContext *)o;
    c->runtime=(NativeBlockStateRuntime *)v((MCObject *)c->runtime,ctx);
    c->replacement=v(c->replacement,ctx);
    c->returned=(Material *)v((MCObject *)c->returned,ctx);
    c->block=(NativeBlock *)v((MCObject *)c->block,ctx);
}
static const MCObjectClass hookClass={"fixture.material.bridge-hooks",MCObjectHeap_plainClone,hook_trace,NULL};
static HookContext *hook_new(NativeBlockStateRuntime *r,bool value) {
    HookContext *c=(HookContext *)MCObjectHeap_alloc(r->object.heap,sizeof(*c),&hookClass);
    CHECK(c);c->runtime=r;c->value=value;c->result=true;return c;
}
static bool property_hook(MCObject *o,Material *captured,bool *out) {
    HookContext *c=(HookContext *)o;
    CHECK(c && captured==c->returned && MCObjectHeap_hasBorrowers(o->heap));
    CHECK(!MCObjectHeap_collect(o->heap));c->calls++;
    *out=c->value;
    if(c->replace)c->runtime->context=c->replacement;
    return c->result;
}
static Material *get_hook(MCObject *o,NativeBlock *captured) {
    HookContext *c=(HookContext *)o;
    CHECK(c && captured==c->block);c->calls++;
    if(c->replace)c->runtime->context=c->replacement;
    if(c->corruptBlockRuntime)captured->runtime=(NativeBlockStateRuntime *)c->replacement;
    if(c->corruptBlockMaterial)captured->material=(Material *)c->replacement;
    return c->returned;
}
static const NativeBlockStateDependencies hooks={.blocksMovement=property_hook,.isLiquid=property_hook,.getMaterial=get_hook};
static void explicit_runtime_hooks(void) {
    MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u);CHECK(h);
    NativeBlockStateRuntime *r=NativeBlockStateRuntime_get(h);MaterialStatics *s=Material_getStatics(h);CHECK(r && s);
    /* Two real managed native runtime owners share the same Source Material.
       A same-heap shallow native registry view is fixture setup, not Source NEW. */
    NativeBlockStateRuntime *other=(NativeBlockStateRuntime *)MCObjectHeap_plainClone(h,(MCObject *)r);CHECK(other);
    HookContext *a=hook_new(r,false),*b=hook_new(other,true);
    a->returned=s->rock;b->returned=s->rock;
    CHECK(NativeBlockStateRuntime_bindDependencies(r,&hooks,(MCObject *)a));
    CHECK(NativeBlockStateRuntime_bindDependencies(other,&hooks,(MCObject *)b));
    bool value=true;
    CHECK(NativeBlockStateRuntime_materialBlocksMovement(r,s->rock,&value) && !value && a->calls==1 && b->calls==0);
    CHECK(NativeBlockStateRuntime_materialIsLiquid(other,s->rock,&value) && value && a->calls==1 && b->calls==1);
    CHECK(Material_blocksMovement(s->rock,&value)==NATIVE_ARRAY_OK && value && a->calls==1 && b->calls==1);
    HookContext *replacement=hook_new(r,true);
    a->replace=true;a->replacement=(MCObject *)replacement;
    CHECK(NativeBlockStateRuntime_materialBlocksMovement(r,s->rock,&value) && !value && r->context==(MCObject *)replacement);
    /* Current traced edges may be legally replaced. The result still comes
       from the captured callback/runtime, without redispatching the new one. */
    a->replace=false;a->block=NativeBlockStateRuntime_block(r,1);
    CHECK(a->block);
    r->context=(MCObject *)a;
    a->corruptBlockRuntime=true;a->replacement=(MCObject *)other;
    CHECK(NativeBlock_getMaterial(a->block)==s->rock && a->block->runtime==other &&
        a->calls==3 && b->calls==1);
    a->block->runtime=r;a->corruptBlockRuntime=false;
    a->corruptBlockMaterial=true;a->replacement=(MCObject *)s->water;
    CHECK(NativeBlock_getMaterial(a->block)==s->rock && a->block->material==s->water &&
        a->calls==4 && b->calls==1);
    CHECK(!MCObjectHeap_failed(h) && !MCObjectHeap_hasBorrowers(h));
    MCObjectHeap_free(h);
}
static void native_failure_guards(void) {
    for(int scenario=0;scenario<10;scenario++) {
        MCObjectHeap *h=MCObjectHeap_new(32u*1024u*1024u),*foreign=MCObjectHeap_new(32u*1024u*1024u);CHECK(h && foreign);
        NativeBlockStateRuntime *r=NativeBlockStateRuntime_get(h);MaterialStatics *s=Material_getStatics(h),*fs=Material_getStatics(foreign);CHECK(r && s && fs);
        HookContext *c=hook_new(r,false);c->returned=s->rock;c->block=NativeBlockStateRuntime_block(r,1);CHECK(c->block);
        CHECK(NativeBlockStateRuntime_bindDependencies(r,&hooks,(MCObject *)c));
        bool out=true;
        if(scenario==0) CHECK(!NativeBlockStateRuntime_materialBlocksMovement(r,fs->rock,&out));
        else if(scenario==1) {
            Material *tiny=(Material *)MCObjectHeap_alloc(h,sizeof(MCObject),Material_nativeClass());CHECK(tiny);
            CHECK(!NativeBlockStateRuntime_materialBlocksMovement(r,tiny,&out));
        } else if(scenario==2) {
            Material fake={0};fake.object.heap=h;fake.object.klass=Material_nativeClass();
            CHECK(!NativeBlockStateRuntime_materialBlocksMovement(r,&fake,&out));
        } else if(scenario==3) {
            CHECK(!NativeBlockStateRuntime_materialBlocksMovement(r,s->rock,NULL));
        } else if(scenario==4 || scenario==5) {
            c->replace=true;c->replacement=(MCObject *)fs->rock;c->result=scenario==4;
            CHECK(!NativeBlockStateRuntime_materialBlocksMovement(r,s->rock,&out));
            CHECK(c->calls==1);
        } else if(scenario==6) {
            MCObject fake={h,&hookClass};c->replace=true;c->replacement=&fake;
            CHECK(!NativeBlockStateRuntime_materialIsLiquid(r,s->rock,&out) && c->calls==1);
        } else if(scenario==7) {
            c->replace=true;c->replacement=(MCObject *)fs->rock;
            CHECK(!NativeBlock_getMaterial(c->block) && c->calls==1);
        } else if(scenario==8) {
            c->corruptBlockRuntime=true;c->replacement=(MCObject *)NativeBlockStateRuntime_get(foreign);
            CHECK(c->replacement && !NativeBlock_getMaterial(c->block) && c->calls==1);
        } else {
            c->corruptBlockMaterial=true;c->replacement=(MCObject *)fs->rock;
            CHECK(!NativeBlock_getMaterial(c->block) && c->calls==1);
        }
        CHECK(out && MCObjectHeap_failed(h) && !MCObjectHeap_failed(foreign));
        CHECK(!MCObjectHeap_hasBorrowers(h));
        MCObjectHeap_free(h);MCObjectHeap_free(foreign);
    }
}
static void dense_native_shape_guards(void) {
    for(int scenario=0;scenario<3;scenario++) {
        mc_world *terrain=calloc(1,sizeof(*terrain));CHECK(terrain);mc_world_init(terrain,19);
        CHECK(mc_world_set(terrain,1,2,3,1u<<4));
        MCGameplay game={0},foreign={0};
        CHECK(MCGameplay_init(&game,32u*1024u*1024u) && MCGameplay_init(&foreign,32u*1024u*1024u));
        MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));
        MCGameplayWorld *world=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),terrain,NULL);CHECK(world);
        BlockPos *pos=BlockPos_newInt(game.heap,1,2,3);CHECK(pos);
        MCObject *chunk=world->dependencies->getChunkFromBlockCoords(world->dependencyContext,world,pos);CHECK(chunk);
        MCObject *block=world->dependencies->chunkGetBlock(world->dependencyContext,chunk,pos);CHECK(block);
        MCObject fake={game.heap,block->klass};
        MCObject *invalid=&fake;
        if(scenario==1) {
            invalid=MCObjectHeap_alloc(game.heap,sizeof(MCObject),block->klass);CHECK(invalid);
        } else if(scenario==2) {
            MCGameplayWorld *fw=MCGameplayWorld_new(foreign.heap,MCGameplay_get(&foreign),terrain,NULL);CHECK(fw);
            BlockPos *fp=BlockPos_newInt(foreign.heap,1,2,3);CHECK(fp);
            MCObject *fc=fw->dependencies->getChunkFromBlockCoords(fw->dependencyContext,fw,fp);CHECK(fc);
            invalid=fw->dependencies->chunkGetBlock(fw->dependencyContext,fc,fp);CHECK(invalid);
        }
        CHECK(!world->dependencies->blockGetMaterial(world->dependencyContext,invalid));
        CHECK(MCObjectHeap_failed(game.heap) && !MCObjectHeap_failed(foreign.heap));
        MCObjectRootScope_end(&scope);CHECK(MCGameplay_free(&game) && MCGameplay_free(&foreign));
        mc_world_free(terrain);free(terrain);
    }
}

static void dense_metadata_and_source_flags(void) {
    mc_world *terrain=calloc(1,sizeof(*terrain));CHECK(terrain);mc_world_init(terrain,11);
    CHECK(mc_world_set(terrain,1,2,3,0));
    MCGameplay game={0};CHECK(MCGameplay_init(&game,32u*1024u*1024u));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,game.heap));
    MCGameplayWorld *world=MCGameplayWorld_new(game.heap,MCGameplay_get(&game),terrain,NULL);CHECK(world);
    BlockPos *pos=BlockPos_newInt(game.heap,1,2,3);CHECK(pos);
    MCObject *chunk=world->dependencies->getChunkFromBlockCoords(world->dependencyContext,world,pos);CHECK(chunk);
    MaterialStatics *s=Material_getStatics(game.heap);Material *expected[198];CHECK(s);expected_materials(s,expected);
    for(int id=0;id<198;id++)for(int meta=0;meta<16;meta++) {
        CHECK(mc_world_set(terrain,1,2,3,(uint16_t)((id<<4)|meta)));
        MCObject *b=world->dependencies->chunkGetBlock(world->dependencyContext,chunk,pos);CHECK(b);
        MCObject *m=world->dependencies->blockGetMaterial(world->dependencyContext,b);
        CHECK(m==(MCObject *)expected[id]);
        bool movement=false,source=false,isLeaves=false;
        CHECK(world->dependencies->materialBlocksMovement(world->dependencyContext,m,&movement));
        CHECK(Material_blocksMovement(expected[id],&source)==NATIVE_ARRAY_OK && movement==source);
        CHECK(world->dependencies->materialIsLeaves(world->dependencyContext,m,&isLeaves) && isLeaves==(id==18 || id==161));
    }
    Material *result=NULL;CHECK(Material_setBurning(s->rock,&result)==NATIVE_ARRAY_OK && result==s->rock);
    CHECK(mc_world_set(terrain,1,2,3,1u<<4));MCObject *b=world->dependencies->chunkGetBlock(world->dependencyContext,chunk,pos);CHECK(b);
    Material *m=(Material *)world->dependencies->blockGetMaterial(world->dependencyContext,b);bool burn=false;
    CHECK(Material_getCanBurn(m,&burn)==NATIVE_ARRAY_OK && burn && m==s->rock);
    CHECK(!MCObjectHeap_failed(game.heap));MCObjectRootScope_end(&scope);
    CHECK(MCGameplay_free(&game));mc_world_free(terrain);free(terrain);
}
static void live_mp_native_admission(void) {
    mc_world *terrain=calloc(1,sizeof(*terrain));CHECK(terrain);mc_world_init(terrain,919);
    for(int z=-1;z<=1;z++)for(int x=-1;x<=1;x++)CHECK(mc_world_chunk(terrain,x,z,true));
    /* A real stone floor covers every ordinary randomized spawn coordinate. */
    for(int z=-16;z<32;z++)for(int x=-16;x<32;x++)CHECK(mc_world_set(terrain,x,0,z,1u<<4));
    MCGameplay game={0};CHECK(mc_server_graph_init(&game,terrain,0,0,919));
    MCGameplayTransaction tx={0};CHECK(MCGameplay_begin(&game,&tx));
    MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,tx.working.heap));
    MCGameplayWorld *world=mc_server_graph_world(&tx.working);CHECK(world);
    size_t beforeObjects=MCObjectHeap_liveObjects(tx.working.heap);
    size_t beforeBytes=MCObjectHeap_liveBytes(tx.working.heap);
    CHECK(mc_server_graph_create_player(&tx.working,0,"11111111-1111-4111-8111-111111111111","MaterialBridge",false));
    MCGameplayPlayer *p=mc_server_graph_player(&tx.working,0);
    CHECK(p && EntityPlayerMP_isInstance((MCObject *)p) && p->living.entity.worldObj==(MCObject *)world);
    printf("native MP source spawn: %.17g %.17g %.17g\n",p->living.entity.posX,p->living.entity.posY,p->living.entity.posZ);
    CHECK(p->living.entity.posY==1.0 && p->living.entity.posX>=-9.5 && p->living.entity.posX<=9.5 &&
        p->living.entity.posZ>=-9.5 && p->living.entity.posZ<=9.5);
    MaterialStatics *s=Material_getStatics(tx.working.heap);CHECK(s);
    CHECK(NativeBlockStateRuntime_materialForBlockId(tx.working.heap,0)==s->air);
    /* Report the real constructor dependency rather than increasing budgets. */
    printf("native MP admission delta: %zu objects, %zu bytes\n",
        MCObjectHeap_liveObjects(tx.working.heap)-beforeObjects,
        MCObjectHeap_liveBytes(tx.working.heap)-beforeBytes);
    CHECK(!MCObjectHeap_failed(tx.working.heap));
    MCObjectRootScope_end(&scope);CHECK(MCGameplay_abort(&tx));CHECK(MCGameplay_free(&game));
    mc_world_free(terrain);free(terrain);
}
int main(void) {
    dense_material_repeated_alias();
    registry_source_aliases_and_methods();
    explicit_runtime_hooks();
    native_failure_guards();
    dense_native_shape_guards();
    dense_metadata_and_source_flags();
    live_mp_native_admission();
    printf("source material bridge: %u checks passed\n",checks);
    return 0;
}
