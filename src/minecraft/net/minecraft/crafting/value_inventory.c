#include "crafting/value_inventory.h"
#include <limits.h>
#include <string.h>
bool mc_value_crafting_attach(mc_value_crafting *view,mc_slot *storage,void *handler,mc_value_notify notify,int width,int height) {
    if (!view || width<0 || height<0 || (height && width>INT_MAX/height) || (width && height && !storage)) return false;
    *view=(mc_value_crafting){storage,width,height,width*height,handler,notify};return true;
}
void mc_value_crafting_free(mc_value_crafting *view) { memset(view,0,sizeof(*view)); }
int mc_value_crafting_getSizeInventory(const mc_value_crafting *view) { return view->size; }
int mc_value_crafting_getWidth(const mc_value_crafting *view) { return view->width; }
int mc_value_crafting_getHeight(const mc_value_crafting *view) { return view->height; }
mc_slot *mc_value_crafting_getStackInSlot(mc_value_crafting *view,int index) {
    return index>=0 && index<view->size && view->stackList[index].item_id>=0 ? &view->stackList[index] : NULL;
}
bool mc_value_crafting_removeStackFromSlot(mc_value_crafting *view,int index,mc_slot *removed) {
    if (!removed || index<0 || index>=view->size) return false;
    for (int i=0;i<view->size;i++) if (removed==&view->stackList[i]) return false;
    mc_slot_free(removed);*removed=view->stackList[index];mc_slot_init(&view->stackList[index]);return true;
}
bool mc_value_crafting_decrStackSize(mc_value_crafting *view,int index,int count,mc_slot *removed) {
    if (index<0 || index>=view->size || count<0 || !removed) return false;
    for (int i=0;i<view->size;i++) if (removed==&view->stackList[i]) return false;
    mc_slot *slot=&view->stackList[index];
    if (slot->item_id<0) { mc_slot_free(removed);return true; }
    if (slot->count<=count) {
        if (!mc_value_crafting_removeStackFromSlot(view,index,removed)) return false;
    } else {
        mc_slot part;mc_slot_init(&part);
        if (!mc_slot_copy(&part,slot)) return false;
        part.count=(uint8_t)count;slot->count=(uint8_t)(slot->count-count);
        mc_slot_free(removed);*removed=part;if (!slot->count)mc_slot_free(slot);
    }
    return view->notify && view->notify(view->eventHandler,view);
}
bool mc_value_crafting_setInventorySlotContents(mc_value_crafting *view,int index,const mc_slot *stack) {
    if (index<0 || index>=view->size) return false;
    mc_slot next;mc_slot_init(&next);
    if (stack && !mc_slot_copy(&next,stack)) return false;
    mc_slot_free(&view->stackList[index]);view->stackList[index]=next;
    return view->notify && view->notify(view->eventHandler,view);
}
void mc_value_result_attach(mc_value_result *view,mc_slot *storage) { view->stackResult=storage; }
void mc_value_result_free(mc_value_result *view) { view->stackResult=NULL; }
void mc_value_result_clear(mc_value_result *view) { mc_slot_free(view->stackResult); }
bool mc_value_result_decrStackSize(mc_value_result *view,int index,int count,mc_slot *removed) {
    (void)index;(void)count;
    if (!removed || removed==view->stackResult) return false;
    mc_slot_free(removed);*removed=*view->stackResult;mc_slot_init(view->stackResult);return true;
}
bool mc_value_result_setInventorySlotContents(mc_value_result *view,int index,const mc_slot *stack) {
    (void)index;if (stack) return mc_slot_copy(view->stackResult,stack);
    mc_slot_free(view->stackResult);return true;
}
