#include "inventory.h"
#include "item/item.h"
#include <string.h>

void mc_slot_init(mc_slot *slot) {
    memset(slot,0,sizeof(*slot)); slot->item_id=-1; mc_nbt_init(&slot->nbt);
}
void mc_slot_free(mc_slot *slot) { mc_nbt_free(&slot->nbt); mc_slot_init(slot); }

static bool valid_slot(const mc_slot *slot) {
    if (!slot) return false;
    if (slot->item_id==-1) return !slot->count && !slot->damage && !slot->nbt.data && !slot->nbt.size;
    if (!mc_item_valid(slot->item_id) || !slot->count || slot->damage<0 ||
        slot->count>127) return false;
    if (!slot->nbt.size) return slot->nbt.data==NULL;
    return slot->nbt.data && slot->nbt.data[0]==10 && mc_nbt_validate(slot->nbt.data,slot->nbt.size);
}
bool mc_slot_copy(mc_slot *destination,const mc_slot *source) {
    if (!destination || !valid_slot(source)) return false;
    if (destination==source) return true;
    mc_slot copied; mc_slot_init(&copied);
    if (!mc_nbt_copy(&copied.nbt,&source->nbt)) return false;
    copied.item_id=source->item_id; copied.count=source->count; copied.damage=source->damage;
    mc_slot_free(destination); *destination=copied; return true;
}
bool mc_slot_equal(const mc_slot *a,const mc_slot *b) {
    return valid_slot(a) && valid_slot(b) && a->item_id==b->item_id && a->count==b->count &&
           a->damage==b->damage && mc_nbt_equal(&a->nbt,&b->nbt);
}
bool mc_slot_can_stack(const mc_slot *a,const mc_slot *b) {
    return valid_slot(a) && valid_slot(b) && a->item_id>=0 && a->item_id==b->item_id &&
           a->damage==b->damage && mc_nbt_equal(&a->nbt,&b->nbt);
}
bool mc_slot_set(mc_slot *slot,int16_t item_id,uint8_t count,int16_t damage) {
    if (!slot) return false;
    if (item_id==-1) {
        if (count || damage) return false;
        mc_slot_free(slot); return true;
    }
    if (!mc_item_valid(item_id) || !count || count>127 || damage<0) return false;
    mc_slot_free(slot); slot->item_id=item_id; slot->count=count; slot->damage=damage; return true;
}
bool mc_slot_read(mc_buf *input,mc_slot *output) {
    if (!input || !output || input->failed) return false;
    size_t start=input->pos; mc_slot decoded; mc_slot_init(&decoded);
    decoded.item_id=mc_get_i16(input);
    if (!input->failed && decoded.item_id!=-1) {
        decoded.count=mc_get_u8(input); decoded.damage=mc_get_i16(input);
        if (!input->failed && !mc_nbt_read(input,&decoded.nbt)) input->failed=true;
    }
    if (input->failed || !valid_slot(&decoded)) {
        input->pos=start; input->failed=true; mc_slot_free(&decoded); return false;
    }
    mc_slot_free(output); *output=decoded; return true;
}
bool mc_slot_write(mc_buf *output,const mc_slot *slot) {
    if (!output) return false;
    if (output->failed || !valid_slot(slot)) { output->failed=true; return false; }
    size_t bytes=slot->item_id==-1 ? 2 : 5+(slot->nbt.size ? slot->nbt.size : 1);
    if (bytes>MC_MAX_PACKET || output->len>MC_MAX_PACKET-bytes) { output->failed=true; return false; }
    size_t start=output->len;
    mc_put_i16(output,slot->item_id);
    if (slot->item_id!=-1) {
        mc_put_u8(output,slot->count); mc_put_i16(output,slot->damage);
        if (!mc_nbt_write(output,&slot->nbt)) output->failed=true;
    }
    if (output->failed) { output->len=start; return false; }
    return true;
}

/* Four consecutive armor IDs per material: helmet, chestplate, leggings, boots. */
static int armor_slot(int16_t item_id) {
    if (item_id>=298 && item_id<=317) return 5+(item_id-298)%4;
    if (item_id==86 || item_id==397) return 5;
    return -1;
}
bool mc_inventory_accepts_slot(int index,const mc_slot *item) {
    if (index<1 || index>=MC_PLAYER_INVENTORY_SIZE || !valid_slot(item)) return false;
    if (item->item_id==-1) return true;
    return index<5 || index>8 || armor_slot(item->item_id)==index;
}
unsigned mc_inventory_slot_limit(int index,const mc_slot *item) {
    if (!mc_inventory_accepts_slot(index,item)) return 0;
    if (index>=5 && index<=8) return 1;
    return item->item_id==-1 ? 64 : mc_item_stack_limit(item->item_id);
}
void mc_inventory_init(mc_inventory *inventory) {
    memset(inventory,0,sizeof(*inventory));
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) mc_slot_init(&inventory->slots[i]);
    mc_slot_init(&inventory->cursor);
}
void mc_inventory_free(mc_inventory *inventory) {
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) mc_slot_free(&inventory->slots[i]);
    mc_slot_free(&inventory->cursor); mc_inventory_init(inventory);
}
static bool valid_inventory(const mc_inventory *inventory) {
    if (!inventory || !valid_slot(&inventory->cursor) || inventory->drag_mode>2 ||
        (inventory->drag_slots>>MC_PLAYER_INVENTORY_SIZE) || (inventory->drag_slots&1)) return false;
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) {
        if (!valid_slot(&inventory->slots[i])) return false;
    }
    return true;
}
bool mc_inventory_copy(mc_inventory *destination,const mc_inventory *source) {
    if (!destination || !valid_inventory(source)) return false;
    if (destination==source) return true;
    mc_inventory copied; mc_inventory_init(&copied);
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) {
        if (!mc_slot_copy(&copied.slots[i],&source->slots[i])) { mc_inventory_free(&copied); return false; }
    }
    if (!mc_slot_copy(&copied.cursor,&source->cursor)) { mc_inventory_free(&copied); return false; }
    copied.drag_active=source->drag_active; copied.drag_mode=source->drag_mode; copied.drag_slots=source->drag_slots;
    mc_inventory_free(destination); *destination=copied; return true;
}

static void reset_drag(mc_inventory *inventory) {
    inventory->drag_active=false; inventory->drag_mode=0; inventory->drag_slots=0;
}
static void swap_slots(mc_slot *a,mc_slot *b) { mc_slot swap=*a; *a=*b; *b=swap; }
static unsigned available_space(unsigned limit,unsigned current) {
    return current<limit ? limit-current : 0;
}
static bool move_count(mc_slot *source,mc_slot *destination,unsigned count) {
    if (!count) return true;
    if (source==destination || count>source->count) return false;
    if (destination->item_id==-1) {
        if (!mc_slot_copy(destination,source)) return false;
        destination->count=(uint8_t)count;
    } else {
        if (!mc_slot_can_stack(source,destination) ||
            count>available_space(mc_item_stack_limit(source->item_id),destination->count)) return false;
        destination->count=(uint8_t)(destination->count+count);
    }
    source->count=(uint8_t)(source->count-count);
    if (!source->count) mc_slot_free(source);
    return true;
}
static bool merge_range(mc_inventory *inventory,mc_slot *source,int start,int end,bool reverse,bool legacy_shift) {
    for (int pass=0;pass<2 && source->item_id!=-1;pass++) {
        for (int i=reverse ? end-1 : start;i>=start && i<end && source->item_id!=-1;i+=reverse ? -1 : 1) {
            mc_slot *destination=&inventory->slots[i];
            if (destination==source || !mc_inventory_accepts_slot(i,source)) continue;
            bool empty=destination->item_id==-1;
            if ((!pass && empty) || (pass && !empty) || (!empty && !mc_slot_can_stack(source,destination))) continue;
            unsigned limit=mc_inventory_slot_limit(i,source);
            unsigned current=empty ? 0 : destination->count;
            /* 1.8.9 shift transfer copies the entire remaining source into its
               first empty target, even for existing tool/armor overstacks.
               Normal clicks, drag and hotbar displacement retain limits. */
            unsigned amount=empty && legacy_shift ? source->count : available_space(limit,current);
            if (amount>source->count) amount=source->count;
            if (!move_count(source,destination,amount)) return false;
        }
    }
    return true;
}
static bool normal_click(mc_inventory *inventory,int index,int button,mc_slot *dropped) {
    mc_slot *cursor=&inventory->cursor;
    if (index==-999) return cursor->item_id==-1 || move_count(cursor,dropped,button ? 1 : cursor->count);
    mc_slot *target=&inventory->slots[index];
    if (cursor->item_id==-1) {
        if (target->item_id==-1) return true;
        return move_count(target,cursor,button ? (target->count+1u)/2u : target->count);
    }
    if (!mc_inventory_accepts_slot(index,cursor)) return true;
    unsigned limit=mc_inventory_slot_limit(index,cursor);
    if (target->item_id==-1 || mc_slot_can_stack(cursor,target)) {
        unsigned current=target->item_id==-1 ? 0 : target->count;
        unsigned amount=available_space(limit,current);
        unsigned requested=button ? 1 : cursor->count;
        if (amount>requested) amount=requested;
        return move_count(cursor,target,amount);
    }
    if (cursor->count<=limit) swap_slots(cursor,target);
    return true;
}
static bool shift_click(mc_inventory *inventory,int index,int button,mc_slot *dropped) {
    if (index==-999) return normal_click(inventory,index,button,dropped);
    mc_slot *source=&inventory->slots[index]; if (source->item_id==-1) return true;
    if (index<=8) return merge_range(inventory,source,9,45,false,true);
    if (source->item_id>=298 && source->item_id<=317) {
        int equip=armor_slot(source->item_id);
        if (inventory->slots[equip].item_id==-1) return merge_range(inventory,source,equip,equip+1,false,true);
    }
    return index<36 ? merge_range(inventory,source,36,45,false,true) : merge_range(inventory,source,9,36,false,true);
}
static int empty_inventory_slot(const mc_inventory *inventory) {
    for (int i=36;i<45;i++) if (inventory->slots[i].item_id==-1) return i;
    for (int i=9;i<36;i++) if (inventory->slots[i].item_id==-1) return i;
    return -1;
}
static bool hotbar_click(mc_inventory *inventory,int index,int button) {
    mc_slot *target=&inventory->slots[index],*hotbar=&inventory->slots[36+button];
    if (target==hotbar) return true;
    if (target->item_id==-1) {
        if (hotbar->item_id!=-1 && mc_inventory_accepts_slot(index,hotbar)) {
            unsigned amount=mc_inventory_slot_limit(index,hotbar); if (amount>hotbar->count) amount=hotbar->count;
            return move_count(hotbar,target,amount);
        }
        return true;
    }
    if (hotbar->item_id==-1 || (index>=5 && mc_inventory_accepts_slot(index,hotbar) &&
        hotbar->count<=mc_inventory_slot_limit(index,hotbar))) { swap_slots(target,hotbar); return true; }
    if (empty_inventory_slot(inventory)<0) return true;
    mc_slot displaced; mc_slot_init(&displaced);
    swap_slots(&displaced,hotbar); swap_slots(target,hotbar);
    bool moved=merge_range(inventory,&displaced,9,45,false,false) && displaced.item_id==-1;
    mc_slot_free(&displaced); return moved;
}
static unsigned selected_count(uint64_t mask) {
    unsigned count=0; while (mask) { count+=(unsigned)(mask&1); mask>>=1; } return count;
}
static bool drag_click(mc_inventory *inventory,int index,int button) {
    unsigned phase=(unsigned)button&3u,mode=(unsigned)button>>2;
    if (phase==0) {
        reset_drag(inventory);
        if (inventory->cursor.item_id!=-1) { inventory->drag_active=true; inventory->drag_mode=mode; }
        return true;
    }
    if (!inventory->drag_active || inventory->drag_mode!=mode) return false;
    if (phase==1) {
        mc_slot *target=&inventory->slots[index]; uint64_t bit=UINT64_C(1)<<index;
        if ((inventory->drag_slots&bit) || !mc_inventory_accepts_slot(index,&inventory->cursor) ||
            (target->item_id!=-1 && !mc_slot_can_stack(target,&inventory->cursor))) return true;
        unsigned current=target->item_id==-1 ? 0 : target->count;
        if (current<mc_inventory_slot_limit(index,&inventory->cursor) &&
            selected_count(inventory->drag_slots)<inventory->cursor.count) inventory->drag_slots|=bit;
        return true;
    }
    unsigned selected=selected_count(inventory->drag_slots);
    if (selected && inventory->cursor.item_id!=-1) {
        mc_slot original; mc_slot_init(&original);
        if (!mc_slot_copy(&original,&inventory->cursor)) return false;
        int remaining=original.count;
        unsigned each=mode==0 ? original.count/selected : 1;
        for (int i=1;i<MC_PLAYER_INVENTORY_SIZE;i++) {
            if (!(inventory->drag_slots&(UINT64_C(1)<<i))) continue;
            mc_slot *target=&inventory->slots[i]; unsigned current=target->item_id==-1 ? 0 : target->count;
            /* A creative update can replace a selected slot before release. */
            if (!mc_inventory_accepts_slot(i,&original) ||
                (target->item_id!=-1 && !mc_slot_can_stack(target,&original))) continue;
            unsigned limit=mc_inventory_slot_limit(i,&original);
            unsigned space=available_space(limit,current);
            unsigned amount=mode==2 ? space : each;
            if (amount>space) amount=space;
            if (mode!=2 && amount>(unsigned)remaining) amount=(unsigned)remaining;
            if (target->item_id==-1 && amount) {
                if (!mc_slot_copy(target,&original)) { mc_slot_free(&original); return false; }
                target->count=0;
            }
            target->count=(uint8_t)(current+amount); remaining-=(int)amount;
        }
        if (remaining<=0) mc_slot_free(&inventory->cursor); else inventory->cursor.count=(uint8_t)remaining;
        mc_slot_free(&original);
    }
    reset_drag(inventory); return true;
}
static bool collect_click(mc_inventory *inventory,int index,int button) {
    mc_slot *cursor=&inventory->cursor;
    if (cursor->item_id==-1 || inventory->slots[index].item_id!=-1) return true;
    unsigned limit=mc_item_stack_limit(cursor->item_id);
    for (int pass=0;pass<2 && cursor->count<limit;pass++) {
        for (int i=button ? 44 : 1;i>=1 && i<45 && cursor->count<limit;i+=button ? -1 : 1) {
            mc_slot *target=&inventory->slots[i];
            if (!mc_slot_can_stack(cursor,target) || (!pass && target->count==mc_item_stack_limit(target->item_id))) continue;
            unsigned amount=limit-cursor->count; if (amount>target->count) amount=target->count;
            if (!move_count(target,cursor,amount)) return false;
        }
    }
    return true;
}
static bool click_valid(int index,int button,int mode) {
    if (mode<0 || mode>6 || (index!=-999 && (index<0 || index>=45))) return false;
    if ((mode==0 || mode==1) && (button==0 || button==1)) return index!=0;
    if (mode==2) return index>=1 && button>=0 && button<9;
    if (mode==3) return index>=1 && button==2;
    if (mode==4) return index>=1 && (button==0 || button==1);
    if (mode==6) return index>=0 && (button==0 || button==1);
    if (mode==5 && button>=0 && button<=10 && (button&3)!=3) {
        int phase=button&3; return phase==1 ? index>=0 : index==-999;
    }
    return false;
}
static bool output_alias(const mc_inventory *inventory,const mc_slot *slot) {
    if (slot==&inventory->cursor) return true;
    for (int i=0;i<MC_PLAYER_INVENTORY_SIZE;i++) if (slot==&inventory->slots[i]) return true;
    return false;
}
bool mc_inventory_click_result(mc_inventory *inventory,int index,int button,int mode,
                               mc_slot *returned,mc_slot *dropped) {
    if (!returned || !dropped || returned==dropped || !valid_inventory(inventory) ||
        output_alias(inventory,returned) || output_alias(inventory,dropped) || !click_valid(index,button,mode)) return false;
    mc_inventory working; mc_inventory_init(&working);
    mc_slot result,drop; mc_slot_init(&result); mc_slot_init(&drop);
    if (!mc_inventory_copy(&working,inventory)) return false;
    bool ok=true,canceled=working.drag_active && mode!=5;
    if (canceled) reset_drag(&working);
    else {
        if ((mode==0 || mode==1) && index>=1 && !mc_slot_copy(&result,&working.slots[index])) ok=false;
        if (ok) switch (mode) {
            case 0: ok=normal_click(&working,index,button,&drop); break;
            case 1: ok=shift_click(&working,index,button,&drop); break;
            case 2: ok=hotbar_click(&working,index,button); break;
            case 3:
                if (working.cursor.item_id==-1 && working.slots[index].item_id!=-1) {
                    ok=mc_slot_copy(&working.cursor,&working.slots[index]);
                    if (ok) working.cursor.count=(uint8_t)mc_item_stack_limit(working.cursor.item_id);
                }
                break;
            case 4:
                if (working.cursor.item_id==-1 && working.slots[index].item_id!=-1)
                    ok=move_count(&working.slots[index],&drop,button ? working.slots[index].count : 1);
                break;
            case 5: ok=drag_click(&working,index,button); break;
            case 6: ok=collect_click(&working,index,button); break;
            default: ok=false; break;
        }
        if (mode==1 && index>=1 && working.slots[index].count==inventory->slots[index].count) mc_slot_free(&result);
    }
    if (ok) ok=valid_inventory(&working);
    if (!ok) { mc_inventory_free(&working); mc_slot_free(&result); mc_slot_free(&drop); return false; }
    mc_inventory_free(inventory); *inventory=working;
    mc_slot_free(returned); *returned=result; mc_slot_free(dropped); *dropped=drop; return true;
}
bool mc_inventory_click(mc_inventory *inventory,int index,int button,int mode,mc_slot *dropped) {
    mc_slot returned; mc_slot_init(&returned);
    bool accepted=mc_inventory_click_result(inventory,index,button,mode,&returned,dropped);
    mc_slot_free(&returned); return accepted;
}
