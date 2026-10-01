#include "item/ItemMapCreated.h"
#include "nbt/NBTTagCompound.h"
#include <string.h>

bool ItemMap_onCreated(ItemStack *stack, MCGameplayWorld *world, MCObject *player) {
    /* The original method does not use playerIn. */
    (void)player;
    if (!stack)
        return false;
    MCObjectHeap *heap = stack->object.heap;
    if (!ItemStack_hasTagCompound(stack) ||
        !NBTTagCompound_getBoolean_ascii(ItemStack_getTagCompound(stack), "map_is_scaling"))
        return !MCObjectHeap_failed(heap);
    if (!MCGameplayWorld_isInstance((MCObject *)world) || world->object.heap != heap) {
        MCObjectHeap_fail(heap);
        return false;
    }
    MCObjectRootScope scope = {0};
    if (!MCObjectRootScope_begin(&scope, heap))
        return false;
    bool ok = MCObjectRootScope_pin(&scope, (MCObject *)stack) &&
              MCObjectRootScope_pin(&scope, (MCObject *)world);
    mc_map_info *old = ok ? ItemMap_getMapData(stack, world) : NULL;
    ok = ok && !MCObjectHeap_failed(heap);
    if (ok) {
        /* Source order matters even when a remote World has no saved map:
           getMapData, allocate ID, change this stack, construct MapData, then
           dereference the old map. A null old map fails after the ID mutation. */
        int32_t id = ItemMapData_getUniqueDataId(world);
        ok = !MCObjectHeap_failed(heap);
        if (ok) {
            ItemStack_setItemDamage(stack, id);
            mc_map_info created = {0};
            created.id = ItemStack_getMetadata(stack);
            if (!old)
                ok = false;
            else {
                uint8_t bits = (uint8_t)(old->scale + 1u);
                int8_t scale;
                memcpy(&scale, &bits, sizeof(scale));
                if (scale > 4)
                    scale = 4;
                created.scale = (uint8_t)scale;
                ItemMapData_calculateMapCenter(&created, (double)old->center_x,
                                               (double)old->center_z, scale);
                created.dimension = old->dimension;
                created.metadata_known = true;
                created.dirty = true;
                /* The store can replace or move old here. All original reads
                   precede setItemData; no borrowed native view survives it. */
                ok = ItemMapData_nativeSetItemData(world, &created) != NULL;
            }
        }
    }
    if (!ok)
        MCObjectHeap_fail(heap);
    MCObjectRootScope_end(&scope);
    return ok && !MCObjectHeap_failed(heap);
}
