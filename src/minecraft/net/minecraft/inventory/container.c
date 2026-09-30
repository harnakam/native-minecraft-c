#include "container.h"
#include <string.h>

unsigned mc_container_slot_count(const mc_container *container) {
    if (!container) return 0;
    return container->kind==MC_CONTAINER_PLAYER ? 45u : container->kind==MC_CONTAINER_WORKBENCH ? 46u : 0u;
}
void mc_container_reset_drag(mc_container *container) {
    container->drag_active=false; container->drag_mode=0; container->drag_slots=0;
}
void mc_container_init(mc_container *container,mc_container_kind kind) {
    memset(container,0,sizeof(*container)); container->kind=kind;
    for (unsigned i=0;i<10;i++) mc_slot_init(&container->slots[i]);
}
void mc_container_free(mc_container *container) {
    mc_container_kind kind=container->kind;
    for (unsigned i=0;i<10;i++) mc_slot_free(&container->slots[i]);
    mc_container_init(container,kind);
}
bool mc_container_copy(mc_container *destination,const mc_container *source) {
    unsigned count=mc_container_slot_count(source);
    if (!destination || !count || source->drag_mode>2 || source->drag_slots>>count || (source->drag_slots&1u)) return false;
    if (destination==source) return true;
    mc_container copied; mc_container_init(&copied,source->kind);
    for (unsigned i=0;i<10;i++) if (!mc_slot_copy(&copied.slots[i],&source->slots[i])) { mc_container_free(&copied); return false; }
    copied.drag_active=source->drag_active; copied.drag_mode=source->drag_mode; copied.drag_slots=source->drag_slots;
    mc_container_free(destination); *destination=copied; return true;
}
mc_slot *mc_container_get(mc_inventory *player,mc_container *container,int index) {
    unsigned count=mc_container_slot_count(container);
    if (!player || index<0 || (unsigned)index>=count) return NULL;
    if (container->kind==MC_CONTAINER_PLAYER) return &player->slots[index];
    if (index<10) return &container->slots[index];
    return &player->slots[index-1];
}
const mc_slot *mc_container_const_get(const mc_inventory *player,const mc_container *container,int index) {
    unsigned count=mc_container_slot_count(container);
    if (!player || index<0 || (unsigned)index>=count) return NULL;
    if (container->kind==MC_CONTAINER_PLAYER) return &player->slots[index];
    return index<10 ? &container->slots[index] : &player->slots[index-1];
}
