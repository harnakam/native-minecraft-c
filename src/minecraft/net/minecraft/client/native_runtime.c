#include "client/native_runtime.h"
#include "entity/player/EntityPlayer.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include "client/native_timer_clock.h"
#include "util/MCGameplayCrafting.h"
#include "item/ItemStackCrafting.h"
#include "item/ItemMapCreated.h"
#include "item/ItemEmptyMap.h"
#include "item/ItemStackUse.h"
#include "entity/player/EntityPlayerDrops.h"
#include "entity/item/NativeItemMotion.h"
#include "entity/player/InventoryPlayerAnimations.h"
#include "item/ItemAnimation.h"
#include "item/ItemMap.h"
#include "nbt/NBTTagCompound.h"
#include "stats/StatFileWriter.h"
#include "stats/StatList.h"
#include "item/item.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trace(MCObject *o, MCObjectVisitor v, void *c) {
    MCClientBindings *b=(MCClientBindings *)o;
    b->player=(MCGameplayPlayer *)v((MCObject *)b->player,c);
    b->controller=(PlayerControllerMP *)v((MCObject *)b->controller,c);
    b->sp=(EntityPlayerSP *)v((MCObject *)b->sp,c);
    b->screenContainer=(Container *)v((MCObject *)b->screenContainer,c);
    b->origin=(DataWatcherBlockPos *)v((MCObject *)b->origin,c);
    b->hotbar=(GuiIngameHotbar *)v((MCObject *)b->hotbar,c);
    b->fontView=v(b->fontView,c);
    b->timer=(Timer *)v((MCObject *)b->timer,c);
}
static const MCObjectClass klass={"C919.native.ClientBindings",MCObjectHeap_plainClone,trace,NULL};
static const MCObjectClass graphics_view={"C919.native.HotbarGraphicsIdentity",MCObjectHeap_plainClone,NULL,NULL};
MCGameplayPlayer *mc_client_graph_player(const MCGameplay *g) {
    MCGameplayObjects *o=g ? (MCGameplayObjects *)MCObjectRoot_get(&g->root) : NULL;
    return o && MCGameplayPlayer_isInstance(o->players[0]) ? (MCGameplayPlayer *)o->players[0] : NULL;
}
MCGameplayWorld *mc_client_graph_world(const MCGameplay *g) {
    MCGameplayObjects *o=g ? (MCGameplayObjects *)MCObjectRoot_get(&g->root) : NULL;
    return o && MCGameplayWorld_isInstance(o->world) ? (MCGameplayWorld *)o->world : NULL;
}
MCClientBindings *mc_client_graph_bindings(const MCGameplay *g) {
    MCGameplayPlayer *p=mc_client_graph_player(g);
    return p && p->effects && p->effects->klass==&klass ? (MCClientBindings *)p->effects : NULL;
}
EntityItem *mc_client_graph_item(const MCGameplay *g,int32_t id) {
    MCGameplayObjects *o=g ? (MCGameplayObjects *)MCObjectRoot_get(&g->root) : NULL;
    if (o) for (size_t i=0;i<o->itemCount;i++)
        if (EntityItem_isInstance(o->items[i]) && ((EntityItem *)o->items[i])->entity.entityId==id) return (EntityItem *)o->items[i];
    return NULL;
}
static MCClientBindings *binding(MCObject *o) {
    if (!o || o->klass!=&klass) { if (o) MCObjectHeap_fail(o->heap); return NULL; }
    return (MCClientBindings *)o;
}
static bool animation(MCObject *c,const Item *item,ItemStack *s,MCObject *w,MCObject *p,int32_t index,bool selected) {
    MCClientBindings *b=binding(c);
    if (!b || !ItemStack_registryIsKnownItem(item) ||
        w!=(MCObject *)((MCGameplayWorld *)(b->player->living.entity.worldObj)) || p!=(MCObject *)b->player) { if (c) MCObjectHeap_fail(c->heap); return false; }
    if (ItemStack_registryId(item)==358) { bool changed; return ItemMap_onUpdate(s,((MCGameplayWorld *)(b->player->living.entity.worldObj)),p,index,selected,&changed); }
    Item_onUpdate(item,s,w,p,index,selected); return !MCObjectHeap_failed(c->heap);
}
static const ItemStackAnimationDependencies item_animation={animation};
static const InventoryPlayerAnimationDependencies inventory_animation={MCGameplayPlayer_world,&item_animation};
bool mc_client_graph_tick_inventory(MCGameplay *g) {
    MCObjectRootScope scope={0}; if (!g || !MCObjectRootScope_begin(&scope,g->heap)) return false;
    MCClientBindings *b=mc_client_graph_bindings(g);
    bool ok=b && ((MCGameplayWorld *)(b->player->living.entity.worldObj))->remote && InventoryPlayer_decrementAnimations(b->player->inventory,&inventory_animation,(MCObject *)b);
    if (!ok) MCObjectHeap_fail(g->heap);
    MCObjectRootScope_end(&scope); return ok && !MCObjectHeap_failed(g->heap);
}
bool mc_client_graph_timer_frame(MCGameplay *g,int32_t *elapsedTicks,float *renderPartialTicks) {
    if (!g || !elapsedTicks || !renderPartialTicks) { if (g) MCObjectHeap_fail(g->heap); return false; }
    MCObjectRootScope scope={0}; if (!MCObjectRootScope_begin(&scope,g->heap)) return false;
    MCClientBindings *b=mc_client_graph_bindings(g);
    bool ok=b && Timer_isInstance((MCObject *)b->timer) && b->timer->object.heap==g->heap && Timer_updateTimer(b->timer);
    if (ok && !MCObjectHeap_failed(g->heap)) {
        *elapsedTicks=b->timer->elapsedTicks; *renderPartialTicks=b->timer->renderPartialTicks;
    } else { MCObjectHeap_fail(g->heap); ok=false; }
    MCObjectRootScope_end(&scope); return ok;
}
bool mc_client_graph_bind_hotbar(MCGameplay *g,const GuiIngameHotbarDependencies *d) {
    MCClientBindings *b=mc_client_graph_bindings(g); if (!b) return false;
    if (b->hotbar) return b->hotbar->dependencies==d;
    MCObject *renderer=MCObjectHeap_alloc(g->heap,sizeof(MCObject),&graphics_view);
    b->fontView=MCObjectHeap_alloc(g->heap,sizeof(MCObject),&graphics_view);
    b->hotbar=renderer && b->fontView ? GuiIngameHotbar_nativeNew(g->heap,(MCObject *)b,renderer,(MCObject *)b,d) : NULL;
    MCObjectHeap_touch(g->heap); return b->hotbar!=NULL;
}
MCObject *mc_client_graph_font_view(MCObject *c,MCObject *mc) {
    MCClientBindings *b=binding(c);
    if (!b || mc!=c) { if (c) MCObjectHeap_fail(c->heap); return NULL; }
    return b->fontView;
}
bool mc_client_graph_render_view(MCObject *c,MCObject *renderer) {
    MCClientBindings *b=binding(c);
    bool valid=b && b->hotbar && renderer==b->hotbar->itemRenderer && renderer && renderer->klass==&graphics_view && renderer->heap==c->heap;
    if (!valid && c) MCObjectHeap_fail(c->heap);
    return valid;
}
static MCPacketThreadResult thread_check(MCObject *c,NetHandlerPlayClient *h,MCObject *p) {
    (void)h; return binding(c) && p && p->heap==c->heap ? MC_PACKET_THREAD_EXECUTE : MC_PACKET_THREAD_FAILED;
}
static MCGameplayPlayer *get_player(MCObject *c,MCObject *game) {
    MCClientBindings *b=binding(c); return b && game==c ? b->player : NULL;
}
static bool creative_screen(MCObject *c,MCObject *game) { MCClientBindings *b=binding(c); return b && game==c && b->creativeScreen; }
static int32_t selected_tab(MCObject *c,MCObject *game) { (void)game; return binding(c) ? 11 : -1; }
static int32_t inventory_tab(MCObject *c) { return binding(c) ? 11 : -1; }
static bool close_screen(MCObject *c,MCGameplayPlayer *p) {
    MCClientBindings *b=binding(c); return b && b->player==p && EntityPlayerSP_closeScreenAndDropStack(b->sp);
}
static bool queue_ack(MCObject *c,NetHandlerPlayClient *h,C0FPacketConfirmTransaction *p) {
    (void)h; MCClientBindings *b=binding(c); return b && MCGameplayClientPackets_addToSendQueue(b->player,(MCObject *)p);
}
static bool queue_click(MCObject *c,NetHandlerPlayClient *h,C0EPacketClickWindow *p) {
    (void)h; MCClientBindings *b=binding(c); return b && MCGameplayClientPackets_addToSendQueue(b->player,(MCObject *)p);
}
static bool queue_any(MCObject *c,NetHandlerPlayClient *h,MCObject *p) {
    (void)h; MCClientBindings *b=binding(c); return b && MCGameplayClientPackets_addToSendQueue(b->player,p);
}
static DataWatcherBlockPos *origin(MCObject *c) { MCClientBindings *b=binding(c); return b ? b->origin : NULL; }
static bool display_null(MCObject *c,MCObject *game) {
    MCClientBindings *b=binding(c); if (!b || game!=c) return false;
    /* Native GUI lifecycle adapter invokes the retained old GuiContainer's
       actual onGuiClosed dependency after SP has reset openContainer. */
    if (b->screenOpen && b->screenContainer && !Container_onContainerClosed(b->screenContainer,b->player->inventory)) return false;
    b->screenContainer=NULL; b->screenOpen=false; b->creativeScreen=false;
    MCObjectHeap_touch(c->heap); return true;
}
static MCObject *get_entity(MCObject *c,MCGameplayWorld *w,int32_t id) {
    if (!binding(c) || !w || w->object.heap!=c->heap) return NULL;
    for(size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++) {
        MCObject *o=w->owners->players[i];
        if(Entity_isInstance(o)&&Entity_getEntityId((Entity *)o)==id)return o;
    }
    for (size_t i=0;i<w->owners->itemCount;i++) {
        MCObject *o=w->owners->items[i];
        if (EntityItem_isInstance(o) && ((EntityItem *)o)->entity.entityId==id) return o;
    }
    return NULL;
}
static DataWatcher *get_watcher(MCObject *c,MCObject *e) {
    return binding(c)&&Entity_isInstance(e)?Entity_getDataWatcher((Entity *)e):NULL;
}
static const NetHandlerPlayClientDependencies handler_deps={
    .checkThreadAndEnqueue=thread_check,.getPlayer=get_player,.isCreativeScreen=creative_screen,
    .selectedCreativeTabIndex=selected_tab,.inventoryCreativeTabIndex=inventory_tab,
    .closeScreenAndDropStack=close_screen,.addToSendQueue=queue_ack,
    .getEntityByID=get_entity,.getDataWatcher=get_watcher};
static const PlayerControllerMPDependencies controller_deps={.addToSendQueue=queue_click};
static bool queue_held(MCObject *c,NetHandlerPlayClient *h,C09PacketHeldItemChange *p) { return queue_any(c,h,(MCObject *)p); }
static bool queue_creative(MCObject *c,NetHandlerPlayClient *h,C10PacketCreativeInventoryAction *p) { return queue_any(c,h,(MCObject *)p); }
static bool queue_place(MCObject *c,NetHandlerPlayClient *h,C08PacketPlayerBlockPlacement *p) { return queue_any(c,h,(MCObject *)p); }
static bool right_click(MCObject *,const Item *,ItemStack *,MCObject *,MCObject *,ItemStack **);
static const ItemStackUseDependencies item_use={right_click};
static const PlayerControllerMPActionsDependencies action_deps={.getPlayer=get_player,
    .addHeldItemToSendQueue=queue_held,.addCreativeItemToSendQueue=queue_creative,
    .addPlacementToSendQueue=queue_place,.itemUse=&item_use};
static const EntityPlayerSPDependencies sp_deps={.addToSendQueue=queue_any,.blockPosOrigin=origin,.displayGuiScreenNull=display_null};
static NativeGameProfile *source_profile(MCObject *context,NetHandlerPlayClient *handler) {
    (void)context;return NetHandlerPlayClient_getGameProfile(handler);
}
static bool walking_actor(MCObject *c,EntityPlayerSP *sp) {
    MCClientBindings *b=binding(c);
    return b && b->sp==sp && EntityPlayerSP_isInstance((MCObject *)sp) &&
        EntityPlayerSP_asPlayer(sp)==b->player && ((MCObject *)sp)->heap==c->heap;
}
static bool walking_sprint(MCObject *c,EntityPlayerSP *sp,bool *out) {
    if(!walking_actor(c,sp)||!out)return false;
    *out=Entity_isSprinting(&EntityPlayerSP_asPlayer(sp)->living.entity);return !MCObjectHeap_failed(c->heap);
}
static bool walking_sneak(MCObject *c,EntityPlayerSP *sp,bool *out) {
    if(!walking_actor(c,sp)||!out)return false;
    *out=EntityPlayerSP_isSneaking(sp);return !MCObjectHeap_failed(c->heap);
}
static bool walking_view(MCObject *c,EntityPlayerSP *sp,bool *out) {
    if(!walking_actor(c,sp)||!out)return false;
    /* Native renderer currently owns this exact local SP view. */
    *out=true;return true;
}
static AxisAlignedBB *walking_box(MCObject *c,EntityPlayerSP *sp) {
    return walking_actor(c,sp)?Entity_getEntityBoundingBox(&EntityPlayerSP_asPlayer(sp)->living.entity):NULL;
}
static const EntityPlayerSPWalkingDependencies walking_deps={walking_sprint,walking_sneak,walking_view,walking_box};
bool mc_client_graph_set_input(MCGameplay *g,float strafe,float forward,bool jump,bool sneak,bool sprint) {
    MCObjectRootScope scope={0};if(!g||!MCObjectRootScope_begin(&scope,g->heap))return false;
    MCClientBindings *b=mc_client_graph_bindings(g);
    bool ok=b && MovementInput_isInstance((MCObject *)b->sp->movementInput);
    if(ok) {
        MovementInput *input=b->sp->movementInput;
        input->moveStrafe=strafe;input->moveForward=forward;input->jump=jump;input->sneak=sneak;
        MCObjectHeap_touch(g->heap);
        if(Entity_isSprinting(&b->player->living.entity)!=sprint)
            ok=EntityLivingBase_setSprinting(&b->player->living,sprint);
    }
    if(!ok)MCObjectHeap_fail(g->heap);
    MCObjectRootScope_end(&scope);return ok&&!MCObjectHeap_failed(g->heap);
}
bool mc_client_graph_tick_walking(MCGameplay *g) {
    MCClientBindings *b=mc_client_graph_bindings(g);
    return b && EntityPlayerSP_onUpdateWalkingPlayer(b->sp);
}

static NBTString *display_name(MCObject *c,const ItemStack *s) {
    if (!c || !s || s->object.heap!=c->heap) return NULL;
    int32_t id=ItemStack_registryId(s->item);
    /* Native Item/localization adapter. Written-book title is its actual
       subclass dispatch; localized renderer resources remain unported. */
    if (id==387 && s->stackTagCompound) {
        NBTString *title=NBTTagCompound_getString_ascii(s->stackTagCompound,"title");
        if (title && NBTString_length(title)) return title;
    }
    return NBTString_fromUTF8(c->heap,id<0 ? "Unknown item" : mc_item_name((int16_t)id));
}
static bool craft_stat(MCObject *o,const Item *item,int32_t amount) {
    MCGameplayPlayer *p=(MCGameplayPlayer *)o;
    if (!MCGameplayPlayer_isInstance(o)) return false;
    MCClientBindings *b=binding(p->effects); int32_t id=ItemStack_registryId(item);
    StatBase *stat=id>=0 && id<(int32_t)MC_GAMEPLAY_CRAFT_STAT_COUNT ? ((MCGameplayWorld *)(p->living.entity.worldObj))->craftStats[id] : NULL;
    return b && EntityPlayerSP_addStat(b->sp,stat,amount);
}
static bool on_created(ItemStack *s,MCObject *w,MCObject *p) {
    if (ItemStack_registryId(s->item)==358) return ItemMap_onCreated(s,(MCGameplayWorld *)w,p);
    /* Original Item.onCreated body is empty; ItemMap is its only override in
       the supplied source. This native immutable subclass dispatch does not
       claim the remaining Item methods translated. */
    return true;
}
static const ItemStackCraftingDispatch item_craft={craft_stat,on_created};
static bool crafted(ItemStack *s,MCObject *w,MCObject *p,int32_t n) { return ItemStack_onCrafting(s,w,p,n,&item_craft); }
static bool achievement(MCObject *o,mc_crafting_achievement which) {
    static const char *ids[]={"achievement.buildWorkBench","achievement.buildPickaxe","achievement.buildFurnace",
        "achievement.buildHoe","achievement.makeBread","achievement.bakeCake","achievement.buildBetterPickaxe",
        "achievement.buildSword","achievement.enchantments","achievement.bookcase","achievement.overpowered"};
    if (!MCGameplayPlayer_isInstance(o) || (unsigned)which>=sizeof ids/sizeof *ids) return false;
    MCGameplayPlayer *p=(MCGameplayPlayer *)o; MCClientBindings *b=binding(p->effects);
    StatBase *stat=StatList_getOneShotStat_ascii(((MCGameplayWorld *)(p->living.entity.worldObj))->statList,ids[which]);
    return b && EntityPlayerSP_triggerAchievement(b->sp,stat);
}
static bool log_missing(MCObject *c,int32_t id) { (void)c; fprintf(stderr,"Remote item %d has no stack metadata\n",id); return true; }
static bool remote(MCObject *c,MCObject *w) { (void)c; return MCGameplayWorld_isRemote(w); }
static InventoryPlayer *entity_inventory(MCObject *c,MCObject *p) {
    return binding(c) && MCGameplayPlayer_isInstance(p) && p->heap==c->heap ? ((MCGameplayPlayer *)p)->inventory : NULL;
}
static const NBTString *entity_name(MCObject *c,MCObject *p) {
    return binding(c) && MCGameplayPlayer_isInstance(p) && p->heap==c->heap ? EntityPlayer_getName((MCGameplayPlayer *)p) : NULL;
}
static MCObject *find_player(MCObject *c,MCObject *world,const NBTString *name) {
    if (!binding(c) || !MCGameplayWorld_isInstance(world) || world->heap!=c->heap) return NULL;
    MCGameplayObjects *owners=((MCGameplayWorld *)world)->owners;
    for (size_t i=0;i<MC_TRANSFER_MAX_PLAYERS;i++) if (MCGameplayPlayer_isInstance(owners->players[i])) {
        MCGameplayPlayer *p=(MCGameplayPlayer *)owners->players[i];
        if (NBTString_equals(EntityPlayer_getName(p),name)) return (MCObject *)p;
    }
    return NULL;
}
static bool entity_achievement(MCObject *c,MCObject *p,EntityItemAchievement which) {
    static const char *ids[]={"achievement.mineWood","achievement.killCow","achievement.diamonds",
        "achievement.blazeRod","achievement.diamondsToYou"};
    MCClientBindings *b=binding(c);
    if (!b || p!=(MCObject *)b->player || (unsigned)which>=sizeof ids/sizeof *ids) return false;
    StatBase *stat=StatList_getOneShotStat_ascii(((MCGameplayWorld *)(b->player->living.entity.worldObj))->statList,ids[which]);
    return stat && EntityPlayerSP_triggerAchievement(b->sp,stat);
}
static bool silent(MCObject *c,const EntityItem *e) {
    if (!binding(c)||!EntityItem_isInstance((const MCObject *)e)||e->entity.object.heap!=c->heap) {
        MCObjectHeap_fail(c?c->heap:NULL);return false;
    }
    return Entity_isSilent((Entity *)&e->entity);
}
static float entity_random(MCObject *c,EntityItem *e) {
    float value=0;
    if (!binding(c)||!EntityItem_isInstance((MCObject *)e)||e->entity.object.heap!=c->heap||
        !e->entity.rand||e->entity.rand->object.heap!=c->heap||!NativeJavaRandom_nextFloat(e->entity.rand,&value)) {
        MCObjectHeap_fail(c?c->heap:NULL);return 0;
    }
    return value;
}
/* Source onCollideWithPlayer returns before these dependencies in a remote
   world. Unported World audio/EntityLivingBase pickup effects fail explicitly
   if a caller ever reaches them; they are not successful empty handlers. */
static bool sound_unported(MCObject *c,MCObject *w,MCObject *p,const char *s,float volume,float pitch) {
    (void)w; (void)p; (void)s; (void)volume; (void)pitch; MCObjectHeap_fail(c->heap); return false;
}
static bool pickup_unported(MCObject *c,MCObject *p,EntityItem *e,int32_t n) {
    (void)p; (void)e; (void)n; MCObjectHeap_fail(c->heap); return false;
}
static bool entity_dead(MCObject *c,EntityItem *e) {
    if (!binding(c) || !e || e->entity.object.heap!=c->heap) return false;
    /* Native inherited Entity.setDead field adapter. */
    e->entity.isDead=true; MCObjectHeap_touch(c->heap); return true;
}
static const EntityItemDependencies entity_deps={.logMissingItem=log_missing,.isRemote=remote,
    .inventory=entity_inventory,.name=entity_name,.findPlayer=find_player,.triggerAchievement=entity_achievement,
    .isSilent=silent,.nextFloat=entity_random,.playSoundAtEntity=sound_unported,
    .onItemPickup=pickup_unported,.setDead=entity_dead};
static bool entity_init(MCObject *c,Entity *e) {
    return binding(c) && EntityItem_entityInit((EntityItem *)e);
}
static bool entity_position(MCObject *c,Entity *e,double x,double y,double z) {
    return binding(c) && Entity_setPosition(e,x,y,z);
}
static bool entity_box(MCObject *c,Entity *e,AxisAlignedBB *box) {
    return binding(c) && Entity_setEntityBoundingBox(e,box);
}
static bool entity_dimension(MCObject *c,MCObject *world,int32_t *out) {
    if (!binding(c) || !MCGameplayWorld_isInstance(world) || !out) return false;
    *out=((MCGameplayWorld *)world)->dimension;return true;
}
static bool entity_remote(MCObject *c,MCObject *world,bool *out) {
    if (!binding(c) || !MCGameplayWorld_isInstance(world) || !out) return false;
    *out=((MCGameplayWorld *)world)->remote;return true;
}
static bool entity_location(MCObject *c,Entity *e,double x,double y,double z,float yaw,float pitch) {
    return binding(c) && Entity_setLocationAndAngles(e,x,y,z,yaw,pitch);
}
static const EntityDependencies entity_base_dependencies={.entityInit=entity_init,.setPosition=entity_position,
    .setEntityBoundingBox=entity_box,.getDimensionId=entity_dimension,.isRemote=entity_remote,.setLocationAndAngles=entity_location};
static bool base_constructor(MCObject *c,EntityItem *e,MCObject *w) {
    return binding(c) && MCGameplayWorld_isInstance(w) &&
        Entity_construct(&e->entity,w,&entity_base_dependencies,c,((MCGameplayWorld *)w)->randomRuntime,NativeEntityIDRuntime_process());
}
static double random_double(MCObject *c) {
    MCClientBindings *b=binding(c);double value=0;
    if (!b||!NativeJavaRandomRuntime_mathRandom(((MCGameplayWorld *)(b->player->living.entity.worldObj))->randomRuntime,&value)) {
        MCObjectHeap_fail(c?c->heap:NULL);return 0;
    }
    return value;
}
static float random_float(MCObject *c,MCGameplayPlayer *p) {
    float value=0;
    if (!binding(c)||!MCGameplayPlayer_isInstance((MCObject *)p)||p->living.entity.object.heap!=c->heap||
        !p->living.entity.rand||p->living.entity.rand->object.heap!=c->heap||!NativeJavaRandom_nextFloat(p->living.entity.rand,&value)) {
        MCObjectHeap_fail(c?c->heap:NULL);return 0;
    }
    return value;
}
static bool size_entity(MCObject *c,EntityItem *e,float w,float h) { return binding(c) && Entity_setSize(&e->entity,w,h); }
static bool position_entity(MCObject *c,EntityItem *e,double x,double y,double z) { return binding(c) && Entity_setPosition(&e->entity,x,y,z); }
static const EntityItemConstructorDependencies constructors={base_constructor,random_double,size_entity,position_entity};
static MCClientBindings *eye_binding(MCObject *c,MCGameplayPlayer *p) {
    MCClientBindings *b=binding(c);
    if(!b||MCObjectHeap_objectSize(c)<sizeof(*b)||b->player!=p||
        !MCGameplayPlayer_isInstance((MCObject *)p)||p->living.entity.object.heap!=c->heap||
        !EntityPlayerSP_isInstance((MCObject *)b->sp)||((MCObject *)b->sp)->heap!=c->heap||
        EntityPlayerSP_asPlayer(b->sp)!=p)return NULL;
    return b;
}
static bool eye_sleeping(MCObject *c,MCGameplayPlayer *p,bool *out) {
    if(!out||!eye_binding(c,p))return false;
    /* EntityPlayer.isPlayerSleeping's original direct field getter. */
    *out=p->sleeping;return true;
}
static bool eye_sneaking(MCObject *c,MCGameplayPlayer *p,bool *out) {
    MCClientBindings *b=eye_binding(c,p);if(!b||!out)return false;
    *out=EntityPlayerSP_isSneaking(b->sp);return !MCObjectHeap_failed(c->heap);
}
static float eye(MCObject *c,MCGameplayPlayer *p) {
    static const EntityPlayerEyeHeightDependencies d={eye_sleeping,eye_sneaking};
    return EntityPlayer_getEyeHeightWithDispatch(p,c,&d);
}
static NBTString *player_name(MCObject *c,MCGameplayPlayer *p) { return binding(c) ? EntityPlayer_getName(p) : NULL; }
static bool join_entity(MCObject *c,MCGameplayPlayer *p,EntityItem *e) {
    MCClientBindings *b=binding(c); return b && b->player==p && EntityPlayerSP_joinEntityItemWithWorld(b->sp,e);
}
static bool drop_stat(MCObject *c,MCGameplayPlayer *p) { MCClientBindings *b=binding(c); return b && EntityPlayerSP_triggerAchievement(b->sp,((MCGameplayWorld *)(p->living.entity.worldObj))->statList->dropStat); }
static double sine(MCObject *c,double v) { (void)c; return sin(v); }
static double cosine(MCObject *c,double v) { (void)c; return cos(v); }
static const EntityPlayerDropsDependencies drops={&entity_deps,&constructors,eye,random_float,player_name,join_entity,drop_stat,sine,cosine};
static bool drop_stack(MCObject *o,ItemStack *s,bool scatter) {
    if (!MCGameplayPlayer_isInstance(o)) return false;
    MCGameplayPlayer *p=(MCGameplayPlayer *)o;
    (void)EntityPlayer_dropPlayerItemWithRandomChoice(p,s,scatter,&drops,p->effects);
    return !MCObjectHeap_failed(o->heap);
}
static StatBase *map_use_stat(MCObject *c,const Item *item) {
    MCClientBindings *b=binding(c);
    if (!b || ItemStack_registryId(item)!=395) { if (c) MCObjectHeap_fail(c->heap); return NULL; }
    StatBase *stat=((MCGameplayWorld *)(b->player->living.entity.worldObj))->emptyMapUseStat;
    if (!stat) MCObjectHeap_fail(c->heap);
    return stat;
}
static bool use_achievement(MCObject *c,MCGameplayPlayer *p,StatBase *stat) {
    MCClientBindings *b=binding(c); return b && b->player==p && EntityPlayerSP_triggerAchievement(b->sp,stat);
}
static bool use_drop(MCObject *c,MCGameplayPlayer *p,ItemStack *s,bool unused,EntityItem **out) {
    if (!binding(c) || !out) return false;
    *out=EntityPlayer_dropPlayerItemWithRandomChoice(p,s,unused,&drops,c);
    return !MCObjectHeap_failed(c->heap);
}
static const ItemEmptyMapDependencies empty_map_deps={map_use_stat,use_achievement,use_drop};
static bool right_click(MCObject *c,const Item *item,ItemStack *s,MCObject *w,MCObject *p,ItemStack **out) {
    if (!binding(c) || !out || ItemStack_registryId(item)!=395 || !MCGameplayWorld_isInstance(w) || !MCGameplayPlayer_isInstance(p)) {
        if (c) MCObjectHeap_fail(c->heap);
        return false; /* Other Item right-click subclasses are explicit dependencies. */
    }
    ItemStack *result=ItemEmptyMap_onItemRightClick(item,s,(MCGameplayWorld *)w,(MCGameplayPlayer *)p,&empty_map_deps,c);
    if (MCObjectHeap_failed(c->heap)) return false;
    *out=result; return true;
}
static mc_crafting_dispatch crafting;
const mc_crafting_dispatch *mc_client_graph_crafting(void) {
    static const MCGameplayCraftingEffects effects={crafted,achievement,drop_stack};
    if (!crafting.inventory) (void)MCGameplayCrafting_nativeDispatch(&crafting,&effects);
    return &crafting;
}
bool mc_client_graph_init(MCGameplay *g,const mc_world *terrain,const char *name) {
    if (!g || !terrain || !name || !MCGameplay_init(g,64u*1024u*1024u)) return false;
    MCObjectRootScope scope={0}; bool ok=MCObjectRootScope_begin(&scope,g->heap);
    MCGameplayWorld *w=ok ? MCGameplayWorld_new(g->heap,MCGameplay_get(g),terrain,NULL) : NULL;
    if (w) {
        w->remote=true;
        SaveDataMemoryStorage *memory=SaveDataMemoryStorage_nativeNewCounterProvider(g->heap);
        if(memory)w->mapStorage=&memory->base;
        ok=memory&&MCGameplay_setWorld(g,(MCObject *)w);
    }
    MCClientBindings *b=ok ? (MCClientBindings *)MCObjectHeap_alloc(g->heap,sizeof(*b),&klass) : NULL;
    if (b) b->timer=Timer_new(g->heap,20.0f,mc_client_timer_clocks(),NULL);
    ok=ok && b && b->timer;
    if (ok && w) ok=MCGameplayCrafting_configureWorld(w,display_name,(MCObject *)b);
    NBTString *n=ok ? NBTString_fromUTF8(g->heap,name) : NULL;
    StatFileWriter *stats=ok ? StatFileWriter_new(g->heap) : NULL;
    uint8_t uuid[16];char text[37];mc_offline_uuid(name,uuid);mc_uuid_string(uuid,text);
    NBTString *idText=n?NBTString_fromASCII(g->heap,text):NULL;
    NativeJavaUUID *id=idText?NativeJavaUUID_fromString(g->heap,idText):NULL;
    NativeGameProfile *profile=id?NativeGameProfile_new(g->heap,id,n):NULL;
    NetHandlerPlayClient *h=profile&&stats&&b?
        NetHandlerPlayClient_nativeBootstrap(g->heap,profile,(MCObject *)b,(MCObject *)b,&handler_deps):NULL;
    EntityPlayerSP *sp=h?EntityPlayerSP_nativeAllocate(g->heap):NULL;
    MCGameplayPlayer *p=EntityPlayerSP_asPlayer(sp);
    if (p && b) {
        /* Parent constructors and their virtual calls operate on this one SP.
           These native aliases and transport bindings hold no second actor. */
        b->player=p;b->sp=sp;p->effects=(MCObject *)b;
        EntityPlayerSPConstructorDependencies constructors={MCGameplayPlayer_nativeConstructorDependencies(),source_profile};
        ok=EntityPlayerSP_construct(sp,(MCObject *)b,(MCObject *)w,h,stats,&constructors,
                mc_client_graph_crafting(),(MCObject *)p,w->randomRuntime,NativeEntityIDRuntime_process()) &&
            MCGameplayPlayer_nativeAttachEnvironment(p,stats) && NetHandlerPlayClient_nativeBindPlayer(h,p) &&
            EntityPlayerSP_bindActions(sp,(MCObject *)b,&sp_deps);
        if(ok) {
            b->origin=DataWatcher_blockPos(g->heap,0,0,0);
            b->controller=PlayerControllerMP_nativeNew(g->heap,h,(MCObject *)b,&controller_deps);
            sp->movementInput=MovementInput_new(g->heap);
            ok=p->gameProfile->id && b->origin && b->controller && sp->movementInput &&
                EntityPlayerSP_bindWalking(sp,(MCObject *)b,&walking_deps) && MCGameplay_setPlayer(g,0,text,(MCObject *)p) && MCGameplayClientPackets_bind(p) &&
                PlayerControllerMP_bindActions(b->controller,(MCObject *)b,(MCObject *)b,&action_deps);
        }
    } else ok=false;
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(g->heap);
}
bool mc_client_graph_open(MCGameplay *g,int32_t window,bool workbench) {
    MCClientBindings *b=mc_client_graph_bindings(g); if (!b) return false;
    if (b->screenOpen && !display_null((MCObject *)b,(MCObject *)b)) return false;
    Container *c=b->player->inventoryContainer;
    if (workbench) {
        ContainerWorkbench *w=ContainerWorkbench_new(b->player->inventory,(MCObject *)((MCGameplayWorld *)(b->player->living.entity.worldObj)),NULL,mc_client_graph_crafting());
        if (!w) return false;
        c=&w->container;
    }
    c->windowId=window; b->player->openContainer=c; b->screenContainer=c; b->screenOpen=true;
    MCObjectHeap_touch(g->heap); return true;
}
bool mc_client_graph_close(MCGameplay *g,bool send) {
    MCClientBindings *b=mc_client_graph_bindings(g); return b && (send ? EntityPlayerSP_closeScreen(b->sp) : EntityPlayerSP_closeScreenAndDropStack(b->sp));
}
bool mc_client_graph_click(MCGameplay *g,int32_t slot,int32_t button,int32_t mode) {
    MCClientBindings *b=mc_client_graph_bindings(g); ItemStack *returned=NULL;
    return b && PlayerControllerMP_windowClick(b->controller,b->player->openContainer->windowId,slot,button,mode,b->player,&returned);
}
bool mc_client_graph_select(MCGameplay *g,int32_t index) {
    MCClientBindings *b=mc_client_graph_bindings(g); if (!b || index<0 || index>=9) return false;
    b->player->inventory->currentItem=index; MCObjectHeap_touch(g->heap);
    return PlayerControllerMP_syncCurrentPlayItem(b->controller);
}
bool mc_client_graph_drop(MCGameplay *g,bool all) {
    MCClientBindings *b=mc_client_graph_bindings(g); EntityItem *out=NULL;
    return b && EntityPlayerSP_dropOneItem(b->sp,all,&out);
}
bool mc_client_graph_creative(MCGameplay *g,int32_t id,int32_t damage) {
    MCClientBindings *b=mc_client_graph_bindings(g); if (!b || !MCGameplayPlayer_isCreativeMode((MCObject *)b->player)) return false;
    ItemStack *s=ItemStack_new(g->heap,ItemStack_registryItem(id),mc_item_stack_limit((int16_t)id),damage);
    if (!s || !InventoryPlayer_setInventorySlotContents(b->player->inventory,b->player->inventory->currentItem,s)) return false;
    /* Explicit native creative catalog action. It sets the real hotbar then
       sends the translated controller action; no confirmed-state mirror. */
    return PlayerControllerMP_sendSlotPacket(b->controller,s,36+b->player->inventory->currentItem);
}
bool mc_client_graph_place(MCGameplay *g,int32_t x,int32_t y,int32_t z,int32_t face,bool air) {
    MCClientBindings *b=mc_client_graph_bindings(g); if (!b || !PlayerControllerMP_syncCurrentPlayItem(b->controller)) return false;
    ItemStack *s=InventoryPlayer_getCurrentItem(b->player->inventory);
    if (air) { bool used=false; return PlayerControllerMP_sendUseItem(b->controller,b->player,((MCGameplayWorld *)(b->player->living.entity.worldObj)),s,&used); }
    DataWatcherBlockPos *pos=air ? NULL : DataWatcher_blockPos(g->heap,x,y,z);
    C08PacketPlayerBlockPlacement *p=air ? C08PacketPlayerBlockPlacement_new_useItem(g->heap,s) :
        pos ? C08PacketPlayerBlockPlacement_new(g->heap,pos,face,s,0.5f,0.5f,0.5f) : NULL;
    return p && MCGameplayClientPackets_addToSendQueue(b->player,(MCObject *)p);
}
static float spawn_angle(int32_t angle) {
    uint32_t product=(uint32_t)angle*360u;int32_t wrapped;
    memcpy(&wrapped,&product,sizeof wrapped);
    return (float)wrapped/256.0f;
}
bool mc_client_graph_spawn_item_packet(MCGameplay *g,int32_t id,double x,double y,double z,
    int32_t pitch,int32_t yaw,int32_t data,double vx,double vy,double vz) {
    MCClientBindings *b=mc_client_graph_bindings(g);
    MCGameplayObjects *o=MCGameplay_get(g); if (!b || !o) return false;
    for (size_t i=0;i<o->itemCount;i++) if (EntityItem_isInstance(o->items[i]) && ((EntityItem *)o->items[i])->entity.entityId==id) {
        if (!MCGameplay_removeItem(g,i)) return false;
        break;
    }
    /* Original item spawn chooses the position constructor. Its inherited
       Entity segment is still an explicit native adapter; packet scalar
       fields/ID/velocity are applied afterward without another stack owner. */
    EntityItem *e=EntityItem_new_position(g->heap,(MCObject *)((MCGameplayWorld *)(b->player->living.entity.worldObj)),(MCObject *)b,
        &entity_deps,&constructors,x,y,z);
    if (!e) return false;
    e->entity.entityId=id;e->entity.rotationPitch=spawn_angle(pitch);e->entity.rotationYaw=spawn_angle(yaw);
    if (data>0) {e->entity.motionX=vx;e->entity.motionY=vy;e->entity.motionZ=vz;}
    MCObjectHeap_touch(g->heap);
    return NativeItemMotion_validate(e) && MCGameplay_addItem(g,(MCObject *)e);
}
bool mc_client_graph_spawn_item(MCGameplay *g,int32_t id,double x,double y,double z,double vx,double vy,double vz) {
    /* Native direct-fixture convenience: explicit zero packet angles and
       positive packet data request the supplied velocity replacement. */
    return mc_client_graph_spawn_item_packet(g,id,x,y,z,0,0,1,vx,vy,vz);
}
static bool any_binding(const MCObject *o,void *context) { (void)o; (void)context; return true; }
void mc_client_graph_slot_name(ItemStack *s,char *out,size_t cap) {
    if (!out || !cap) return;
    if (!s) { snprintf(out,cap,"Empty"); return; }
    if (!s->item) { snprintf(out,cap,"Unknown item"); return; }
    MCClientBindings *b=(MCClientBindings *)MCObjectHeap_findObject(s->object.heap,&klass,any_binding,NULL);
    NBTString *name=b ? ItemStack_getDisplayName(s,display_name,(MCObject *)b) : NULL;
    size_t fullCap=name ? NBTString_length(name)*3u+1u : 0;
    char *full=fullCap ? malloc(fullCap) : NULL;
    if (full && NBTString_toUTF8(name,full,fullCap)) {
        size_t n=strlen(full); if (n>=cap) n=cap-1;
        while (n && ((unsigned char)full[n]&0xc0u)==0x80u) --n;
        memcpy(out,full,n); out[n]=0;
    } else snprintf(out,cap,"%s",mc_item_name((int16_t)ItemStack_registryId(s->item)));
    free(full);
}
