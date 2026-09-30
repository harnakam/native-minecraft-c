#include "inventory/InventoryCrafting.h"
#include "inventory/InventoryCraftResult.h"
#include "crafting/crafting.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#value); exit(1); } } while (0)
typedef struct { unsigned calls; int seen[16]; bool reject; } observer;
static bool notify(void *handler,InventoryCrafting *inventory) {
    observer *state=handler;
    mc_slot *stack=InventoryCrafting_getStackInSlot(inventory,0);
    CHECK(state->calls<16); state->seen[state->calls++]=stack ? stack->count : -1;
    return !state->reject;
}
static void matrix_methods(void) {
    observer state={0}; InventoryCrafting inventory; CHECK(InventoryCrafting_init(&inventory,&state,notify,3,3));
    CHECK(inventory.ownsStackList && InventoryCrafting_getSizeInventory(&inventory)==9);
    CHECK(InventoryCrafting_getWidth(&inventory)==3 && InventoryCrafting_getHeight(&inventory)==3);
    CHECK(!state.calls && !InventoryCrafting_getStackInSlot(&inventory,0));
    CHECK(!strcmp(InventoryCrafting_getName(&inventory),"container.crafting") && !InventoryCrafting_hasCustomName(&inventory));
    InventoryDisplayName display=InventoryCrafting_getDisplayName(&inventory); CHECK(display.translated && !strcmp(display.key,"container.crafting"));
    CHECK(InventoryCrafting_getInventoryStackLimit(&inventory)==64 && InventoryCrafting_isUseableByPlayer(&inventory,NULL));
    CHECK(InventoryCrafting_isItemValidForSlot(&inventory,-99,NULL));
    InventoryCrafting_markDirty(&inventory); InventoryCrafting_openInventory(&inventory,NULL); InventoryCrafting_closeInventory(&inventory,NULL);
    InventoryCrafting_setField(&inventory,1,12); CHECK(!InventoryCrafting_getField(&inventory,1) && !InventoryCrafting_getFieldCount(&inventory) && !state.calls);
    mc_slot stack,removed; mc_slot_init(&stack); mc_slot_init(&removed); CHECK(mc_slot_set(&stack,17,5,0));
    CHECK(InventoryCrafting_setInventorySlotContents(&inventory,0,&stack)); CHECK(state.calls==1 && state.seen[0]==5);
    CHECK(InventoryCrafting_getStackInRowAndColumn(&inventory,0,0)==&inventory.stackList[0]);
    CHECK(!InventoryCrafting_getStackInRowAndColumn(&inventory,3,0) && !InventoryCrafting_getStackInRowAndColumn(&inventory,0,3));
    CHECK(!inventory.failed && !InventoryCrafting_getStackInRowAndColumn(&inventory,-1,0));
    CHECK(InventoryCrafting_decrStackSize(&inventory,0,2,&removed)); CHECK(removed.count==2 && inventory.stackList[0].count==3 && state.calls==2 && state.seen[1]==3);
    CHECK(InventoryCrafting_decrStackSize(&inventory,0,0,&removed)); CHECK(removed.item_id==17 && !removed.count && state.calls==3 && state.seen[2]==3);
    CHECK(InventoryCrafting_decrStackSize(&inventory,0,4,&removed)); CHECK(removed.count==3 && !InventoryCrafting_getStackInSlot(&inventory,0) && state.calls==4 && state.seen[3]==-1);
    CHECK(InventoryCrafting_decrStackSize(&inventory,0,1,&removed)); CHECK(removed.item_id==-1 && state.calls==4);
    CHECK(InventoryCrafting_setInventorySlotContents(&inventory,0,NULL)); CHECK(state.calls==5 && state.seen[4]==-1);
    CHECK(InventoryCrafting_setInventorySlotContents(&inventory,0,&stack)); CHECK(InventoryCrafting_removeStackFromSlot(&inventory,0,&removed)); CHECK(removed.count==5 && state.calls==6);
    CHECK(InventoryCrafting_setInventorySlotContents(&inventory,0,&stack)); InventoryCrafting_clear(&inventory); CHECK(state.calls==7 && !InventoryCrafting_getStackInSlot(&inventory,0));
    CHECK(!InventoryCrafting_removeStackFromSlot(&inventory,0,&inventory.stackList[1]));
    state.reject=true; CHECK(!InventoryCrafting_setInventorySlotContents(&inventory,0,&stack)); CHECK(inventory.failed && state.calls==8 && inventory.stackList[0].count==5);
    InventoryCrafting_free(&inventory); mc_slot_free(&stack); mc_slot_free(&removed);
    CHECK(InventoryCrafting_init(&inventory,&state,notify,0,3)); CHECK(!InventoryCrafting_getSizeInventory(&inventory)); InventoryCrafting_free(&inventory);
    CHECK(!InventoryCrafting_init(&inventory,&state,notify,INT_MAX,2));
}
static void attached_and_result(void) {
    mc_inventory player; mc_inventory_init(&player); observer state={0}; InventoryCrafting matrix;
    CHECK(InventoryCrafting_attach(&matrix,&player.slots[1],&state,notify,2,2)); CHECK(!matrix.ownsStackList);
    mc_slot stack,removed; mc_slot_init(&stack); mc_slot_init(&removed); CHECK(mc_slot_set(&stack,276,2,0));
    CHECK(InventoryCrafting_setInventorySlotContents(&matrix,3,&stack)); CHECK(player.slots[4].count==2);
    InventoryCrafting_free(&matrix); CHECK(player.slots[4].count==2); /* Attached storage is still player's owner. */
    InventoryCraftResult result; InventoryCraftResult_init(&result);
    CHECK(InventoryCraftResult_getSizeInventory(&result)==1 && !InventoryCraftResult_getStackInSlot(&result,99));
    CHECK(!strcmp(InventoryCraftResult_getName(&result),"Result") && !InventoryCraftResult_hasCustomName(&result));
    InventoryDisplayName display=InventoryCraftResult_getDisplayName(&result); CHECK(display.translated && !strcmp(display.key,"Result"));
    CHECK(InventoryCraftResult_setInventorySlotContents(&result,-99,&stack)); CHECK(InventoryCraftResult_getStackInSlot(&result,99)->count==2);
    CHECK(InventoryCraftResult_decrStackSize(&result,99,1,&removed)); CHECK(removed.count==2 && !InventoryCraftResult_getStackInSlot(&result,0));
    CHECK(InventoryCraftResult_setInventorySlotContents(&result,0,&stack)); CHECK(InventoryCraftResult_removeStackFromSlot(&result,99,&removed)); CHECK(removed.count==2);
    CHECK(InventoryCraftResult_getInventoryStackLimit(&result)==64 && InventoryCraftResult_isUseableByPlayer(&result,NULL) && InventoryCraftResult_isItemValidForSlot(&result,0,&stack));
    InventoryCraftResult_markDirty(&result); InventoryCraftResult_openInventory(&result,NULL); InventoryCraftResult_closeInventory(&result,NULL);
    InventoryCraftResult_setField(&result,99,12); CHECK(!InventoryCraftResult_getField(&result,99) && !InventoryCraftResult_getFieldCount(&result));
    CHECK(InventoryCraftResult_setInventorySlotContents(&result,0,&stack)); InventoryCraftResult_clear(&result); CHECK(!InventoryCraftResult_getStackInSlot(&result,0)); InventoryCraftResult_free(&result);
    InventoryCraftResult_attach(&result,&player.slots[0]); CHECK(InventoryCraftResult_setInventorySlotContents(&result,0,&stack)); InventoryCraftResult_free(&result); CHECK(player.slots[0].count==2);
    mc_slot_free(&stack); mc_slot_free(&removed); mc_inventory_free(&player);
}
static bool update_result(void *handler,InventoryCrafting *matrix) {
    InventoryCraftResult *result=handler; mc_slot output,left[9]; mc_slot_init(&output);
    unsigned size=(unsigned)InventoryCrafting_getSizeInventory(matrix); for (unsigned i=0;i<size;i++) mc_slot_init(&left[i]);
    bool okay=mc_crafting_match(matrix->stackList,(unsigned)InventoryCrafting_getWidth(matrix),(unsigned)InventoryCrafting_getHeight(matrix),&output,left);
    if (okay) okay=InventoryCraftResult_setInventorySlotContents(result,0,&output);
    mc_slot_free(&output); for (unsigned i=0;i<size;i++) mc_slot_free(&left[i]); return okay;
}
static void immediate_recipe_notifications(void) {
    InventoryCraftResult result; InventoryCraftResult_init(&result); InventoryCrafting matrix;
    CHECK(InventoryCrafting_init(&matrix,&result,update_result,2,2)); mc_slot log,removed; mc_slot_init(&log); mc_slot_init(&removed); CHECK(mc_slot_set(&log,17,2,0));
    CHECK(InventoryCrafting_setInventorySlotContents(&matrix,0,&log)); CHECK(InventoryCraftResult_getStackInSlot(&result,0)->item_id==5);
    CHECK(InventoryCrafting_decrStackSize(&matrix,0,1,&removed)); CHECK(matrix.stackList[0].count==1 && InventoryCraftResult_getStackInSlot(&result,0)->count==4);
    CHECK(InventoryCrafting_decrStackSize(&matrix,0,1,&removed)); CHECK(!InventoryCraftResult_getStackInSlot(&result,0));
    CHECK(InventoryCrafting_setInventorySlotContents(&matrix,0,&log)); CHECK(InventoryCrafting_removeStackFromSlot(&matrix,0,&removed));
    CHECK(InventoryCraftResult_getStackInSlot(&result,0)->item_id==5); /* remove deliberately does not notify. */
    InventoryCrafting_free(&matrix); InventoryCraftResult_free(&result); mc_slot_free(&log); mc_slot_free(&removed);
}
int main(void) { matrix_methods(); attached_and_result(); immediate_recipe_notifications(); printf("InventoryCrafting source port: %u checks passed\n",checks); return 0; }
