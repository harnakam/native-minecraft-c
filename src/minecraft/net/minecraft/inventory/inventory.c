#include "container_runtime.h"
#include "item/item.h"
#include <limits.h>
#include <math.h>
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

typedef struct {
    mc_slot *slots[46], *cursor;
    unsigned count, main_start, hotbar_start;
    bool player_kind, *drag_active;
    unsigned *drag_mode;
    uint64_t *drag_slots;
} inventory_view;
static void player_view(inventory_view *view,mc_inventory *player) {
    memset(view,0,sizeof(*view)); view->count=45; view->main_start=9; view->hotbar_start=36; view->player_kind=true;
    for (unsigned i=0;i<45;i++) view->slots[i]=&player->slots[i];
    view->cursor=&player->cursor; view->drag_active=&player->drag_active; view->drag_mode=&player->drag_mode; view->drag_slots=&player->drag_slots;
}
static void container_view(inventory_view *view,mc_inventory *player,mc_container *container) {
    player_view(view,player); view->count=mc_container_slot_count(container); view->player_kind=container->kind==MC_CONTAINER_PLAYER;
    view->main_start=view->player_kind ? 9 : 10; view->hotbar_start=view->player_kind ? 36 : 37;
    for (unsigned i=0;i<view->count;i++) view->slots[i]=mc_container_get(player,container,(int)i);
    view->drag_active=&container->drag_active; view->drag_mode=&container->drag_mode; view->drag_slots=&container->drag_slots;
}
static bool view_accepts_slot(const inventory_view *view,int index,const mc_slot *item) {
    if (index<1 || (unsigned)index>=view->count || !valid_slot(item)) return false;
    return !view->player_kind || mc_inventory_accepts_slot(index,item);
}
static unsigned view_slot_limit(const inventory_view *view,int index,const mc_slot *item) {
    if (!view_accepts_slot(view,index,item)) return 0;
    return view->player_kind ? mc_inventory_slot_limit(index,item) : item->item_id<0 ? 64u : mc_item_stack_limit(item->item_id);
}
static void reset_drag(inventory_view *inventory) {
    (*inventory->drag_active)=false; (*inventory->drag_mode)=0; (*inventory->drag_slots)=0;
}
static void swap_slots(mc_slot *a,mc_slot *b) { mc_slot swap=*a; *a=*b; *b=swap; }
static unsigned available_space(unsigned limit,unsigned current) {
    return current<limit ? limit-current : 0;
}
static bool shift_can_stack(const mc_slot *a,const mc_slot *b) {
    return a->item_id>=0 && a->item_id==b->item_id &&
           (!mc_item_has_subtypes(a->item_id) || a->damage==b->damage) && mc_nbt_equal(&a->nbt,&b->nbt);
}
static bool move_count_context(mc_slot *source,mc_slot *destination,unsigned count,bool legacy_merge) {
    if (!count) return true;
    if (source==destination || count>source->count) return false;
    if (destination->item_id==-1) {
        if (!mc_slot_copy(destination,source)) return false;
        destination->count=(uint8_t)count;
    } else {
        if (!(legacy_merge ? shift_can_stack(source,destination) : mc_slot_can_stack(source,destination)) ||
            count>available_space(mc_item_stack_limit(source->item_id),destination->count)) return false;
        destination->count=(uint8_t)(destination->count+count);
    }
    source->count=(uint8_t)(source->count-count);
    if (!source->count) mc_slot_free(source);
    return true;
}
static bool move_count(mc_slot *source,mc_slot *destination,unsigned count) {
    return move_count_context(source,destination,count,false);
}
static bool merge_range(inventory_view *inventory,mc_slot *source,int start,int end,bool reverse,bool legacy_shift) {
    for (int pass=0;pass<2 && source->item_id!=-1;pass++) {
        for (int i=reverse ? end-1 : start;i>=start && i<end && source->item_id!=-1;i+=reverse ? -1 : 1) {
            mc_slot *destination=inventory->slots[i];
            if (destination==source || !view_accepts_slot(inventory,i,source)) continue;
            bool empty=destination->item_id==-1;
            if ((!pass && empty) || (pass && !empty) || (!empty && !shift_can_stack(source,destination))) continue;
            unsigned limit=view_slot_limit(inventory,i,source);
            unsigned current=empty ? 0 : destination->count;
            /* 1.8.9 shift transfer copies the entire remaining source into its
               first empty target, even for existing tool/armor overstacks.
               Normal clicks, drag and hotbar displacement retain limits. */
            unsigned amount=empty && legacy_shift ? source->count : available_space(limit,current);
            if (amount>source->count) amount=source->count;
            if (!move_count_context(source,destination,amount,true)) return false;
        }
    }
    return true;
}
static bool normal_click(inventory_view *inventory,int index,int button,mc_slot *dropped) {
    mc_slot *cursor=inventory->cursor;
    if (index==-999) return cursor->item_id==-1 || move_count(cursor,dropped,button ? 1 : cursor->count);
    mc_slot *target=inventory->slots[index];
    if (cursor->item_id==-1) {
        if (target->item_id==-1) return true;
        return move_count(target,cursor,button ? (target->count+1u)/2u : target->count);
    }
    if (!view_accepts_slot(inventory,index,cursor)) return true;
    unsigned limit=view_slot_limit(inventory,index,cursor);
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
static bool shift_click(inventory_view *inventory,int index,int button,mc_slot *dropped) {
    if (index==-999) return normal_click(inventory,index,button,dropped);
    mc_slot *source=inventory->slots[index]; if (source->item_id==-1) return true;
    if ((unsigned)index<inventory->main_start) return merge_range(inventory,source,(int)inventory->main_start,(int)inventory->count,false,true);
    if (inventory->player_kind && source->item_id>=298 && source->item_id<=317) {
        int equip=armor_slot(source->item_id);
        if (inventory->slots[equip]->item_id==-1) return merge_range(inventory,source,equip,equip+1,false,true);
    }
    return (unsigned)index<inventory->hotbar_start ? merge_range(inventory,source,(int)inventory->hotbar_start,(int)inventory->count,false,true) : merge_range(inventory,source,(int)inventory->main_start,(int)inventory->hotbar_start,false,true);
}
static int empty_inventory_slot(const inventory_view *inventory) {
    for (int i=(int)inventory->hotbar_start;i<(int)inventory->count;i++) if (inventory->slots[i]->item_id==-1) return i;
    for (int i=(int)inventory->main_start;i<(int)inventory->hotbar_start;i++) if (inventory->slots[i]->item_id==-1) return i;
    return -1;
}
static bool damaged_item(const mc_slot *slot) {
    int id=slot->item_id;
    bool damageable=(id>=256 && id<=259) || id==261 || (id>=267 && id<=279) ||
        (id>=283 && id<=286) || (id>=290 && id<=294) || (id>=298 && id<=317) || id==346 || id==359 || id==398;
    if (!damageable || !slot->damage) return false;
    mc_nbt_view root,field; int64_t value;
    if (slot->nbt.size && mc_nbt_root(&slot->nbt,&root) && mc_nbt_find(&root,"Unbreakable",&field)) {
        if (mc_nbt_get_integer(&field,&value) && (uint8_t)value!=0) return false;
        double number;
        if ((field.type==5 || field.type==6) && mc_nbt_get_number(&field,&number)) {
            int32_t integral=isnan(number) ? 0 : number>=INT32_MAX ? INT32_MAX : number<=INT32_MIN ? INT32_MIN : (int32_t)number;
            uint32_t floor_value=(uint32_t)integral; if (number<integral) --floor_value;
            if ((floor_value&255u)!=0) return false;
        }
    }
    return true;
}
static bool insert_view(inventory_view *inventory,mc_slot *source) {
    if (source->item_id<0) return true;
    if (damaged_item(source)) {
        int empty=empty_inventory_slot(inventory);
        return empty<0 || move_count(source,inventory->slots[empty],source->count);
    }
    unsigned hotbar=inventory->count-inventory->hotbar_start,total=inventory->count-inventory->main_start;
    for (unsigned pass=0;pass<2 && source->item_id!=-1;pass++) for (unsigned j=0;j<total && source->item_id!=-1;j++) {
        unsigned index=j<hotbar ? inventory->hotbar_start+j : inventory->main_start+j-hotbar;
        mc_slot *target=inventory->slots[index]; bool empty=target->item_id==-1;
        if ((pass==0 && empty) || (pass==1 && !empty) || (!empty && !shift_can_stack(source,target))) continue;
        unsigned limit=mc_item_stack_limit(source->item_id),room=empty ? limit : available_space(limit,target->count);
        unsigned amount=source->count<room ? source->count : room;
        if (!move_count_context(source,target,amount,true)) return false;
    }
    return true;
}
bool mc_inventory_insert(mc_inventory *inventory,mc_slot *item) {
    if (!valid_inventory(inventory) || !valid_slot(item) || item==&inventory->cursor) return false;
    for (unsigned i=0;i<45;i++) if (item==&inventory->slots[i]) return false;
    mc_inventory next; mc_inventory_init(&next); mc_slot remaining; mc_slot_init(&remaining);
    bool ok=mc_inventory_copy(&next,inventory) && mc_slot_copy(&remaining,item);
    inventory_view view; player_view(&view,&next);
    if (ok) ok=insert_view(&view,&remaining);
    if (ok) {
        mc_inventory_free(inventory); *inventory=next; mc_inventory_init(&next);
        mc_slot_free(item); *item=remaining; mc_slot_init(&remaining);
    }
    mc_inventory_free(&next); mc_slot_free(&remaining); return ok;
}
static bool hotbar_click(inventory_view *inventory,int index,int button) {
    mc_slot *target=inventory->slots[index],*hotbar=inventory->slots[inventory->hotbar_start+(unsigned)button];
    if (target==hotbar) return true;
    if (target->item_id==-1) {
        if (hotbar->item_id!=-1 && view_accepts_slot(inventory,index,hotbar)) {
            return move_count(hotbar,target,hotbar->count);
        }
        return true;
    }
    bool player_owner=inventory->player_kind ? index>=5 : (unsigned)index>=inventory->main_start;
    if (hotbar->item_id==-1 || (player_owner && view_accepts_slot(inventory,index,hotbar))) { swap_slots(target,hotbar); return true; }
    if (empty_inventory_slot(inventory)<0) return true;
    mc_slot displaced; mc_slot_init(&displaced);
    swap_slots(&displaced,hotbar); swap_slots(target,hotbar);
    /* Vanilla ignores addItemStackToInventory's leftover here, including
       oversized undamaged tools. This legacy loss is separate from errors. */
    bool moved=insert_view(inventory,&displaced);
    mc_slot_free(&displaced); return moved;
}
static unsigned selected_count(uint64_t mask) {
    unsigned count=0; while (mask) { count+=(unsigned)(mask&1); mask>>=1; } return count;
}
static bool drag_click(inventory_view *inventory,int index,int button) {
    unsigned phase=(unsigned)button&3u,mode=(unsigned)button>>2;
    if (phase==0) {
        reset_drag(inventory);
        if (inventory->cursor->item_id!=-1) { (*inventory->drag_active)=true; (*inventory->drag_mode)=mode; }
        return true;
    }
    if (!(*inventory->drag_active) || (*inventory->drag_mode)!=mode) return false;
    if (phase==1) {
        mc_slot *target=inventory->slots[index]; uint64_t bit=UINT64_C(1)<<index;
        if (((*inventory->drag_slots)&bit) || !view_accepts_slot(inventory,index,inventory->cursor) ||
            (target->item_id!=-1 && !mc_slot_can_stack(target,inventory->cursor))) return true;
        unsigned current=target->item_id==-1 ? 0 : target->count;
        if (current<view_slot_limit(inventory,index,inventory->cursor) &&
            selected_count((*inventory->drag_slots))<inventory->cursor->count) (*inventory->drag_slots)|=bit;
        return true;
    }
    unsigned selected=selected_count((*inventory->drag_slots));
    if (selected && inventory->cursor->item_id!=-1) {
        mc_slot original; mc_slot_init(&original);
        if (!mc_slot_copy(&original,inventory->cursor)) return false;
        int remaining=original.count;
        unsigned each=mode==0 ? original.count/selected : 1;
        for (int i=1;i<(int)inventory->count;i++) {
            if (!((*inventory->drag_slots)&(UINT64_C(1)<<i))) continue;
            mc_slot *target=inventory->slots[i]; unsigned current=target->item_id==-1 ? 0 : target->count;
            /* A creative update can replace a selected slot before release. */
            if (!view_accepts_slot(inventory,i,&original) ||
                (target->item_id!=-1 && !mc_slot_can_stack(target,&original))) continue;
            unsigned limit=view_slot_limit(inventory,i,&original);
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
        if (remaining<=0) mc_slot_free(inventory->cursor); else inventory->cursor->count=(uint8_t)remaining;
        mc_slot_free(&original);
    }
    reset_drag(inventory); return true;
}
static bool collect_click(inventory_view *inventory,int index,int button) {
    mc_slot *cursor=inventory->cursor;
    if (cursor->item_id==-1 || inventory->slots[index]->item_id!=-1) return true;
    unsigned limit=mc_item_stack_limit(cursor->item_id);
    for (int pass=0;pass<2 && cursor->count<limit;pass++) {
        for (int i=button ? (int)inventory->count-1 : 1;i>=1 && i<(int)inventory->count && cursor->count<limit;i+=button ? -1 : 1) {
            mc_slot *target=inventory->slots[i];
            if (!mc_slot_can_stack(cursor,target) || (!pass && target->count==mc_item_stack_limit(target->item_id))) continue;
            unsigned amount=limit-cursor->count; if (amount>target->count) amount=target->count;
            if (!move_count(target,cursor,amount)) return false;
        }
    }
    return true;
}
static bool click_valid(unsigned count,int index,int button,int mode) {
    if (mode<0 || mode>6 || (index!=-999 && (index<0 || (unsigned)index>=count))) return false;
    if ((mode==0 || mode==1) && (button==0 || button==1)) return index!=0;
    if (mode==2) return index>=1 && button>=0 && button<9;
    if (mode==3) return index>=1;
    if (mode==4) return index>=1 && (button==0 || button==1);
    if (mode==6) return index>=0 && (button==0 || button==1);
    if (mode==5 && button>=0 && button<=10 && (button&3)!=3) {
        int phase=button&3; return phase==1 ? index>=0 : index==-999;
    }
    return false;
}
static bool view_alias(const inventory_view *view,const mc_slot *slot) {
    if (slot==view->cursor) return true;
    for (unsigned i=0;i<view->count;i++) if (slot==view->slots[i]) return true;
    return false;
}
static bool execute_click(inventory_view *view,int index,int button,int mode,mc_slot *result,mc_slot *drop) {
    if (*view->drag_active && mode!=5) { reset_drag(view); return true; }
    unsigned original= index>=1 ? view->slots[index]->count : 0;
    if ((mode==0 || mode==1) && index>=1 && !mc_slot_copy(result,view->slots[index])) return false;
    bool ok=true;
    switch (mode) {
        case 0: ok=normal_click(view,index,button,drop); break;
        case 1: ok=shift_click(view,index,button,drop); break;
        case 2: ok=hotbar_click(view,index,button); break;
        case 3:
            if (view->cursor->item_id==-1 && view->slots[index]->item_id!=-1) {
                ok=mc_slot_copy(view->cursor,view->slots[index]);
                if (ok) view->cursor->count=(uint8_t)mc_item_stack_limit(view->cursor->item_id);
            }
            break;
        case 4:
            if (view->cursor->item_id==-1 && view->slots[index]->item_id!=-1)
                ok=move_count(view->slots[index],drop,button ? view->slots[index]->count : 1);
            break;
        case 5: ok=drag_click(view,index,button); break;
        case 6: ok=collect_click(view,index,button); break;
        default: ok=false; break;
    }
    if (mode==1 && index>=1 && view->slots[index]->count==original) mc_slot_free(result);
    return ok;
}
bool mc_inventory_click_result(mc_inventory *inventory,int index,int button,int mode,mc_slot *returned,mc_slot *dropped) {
    if (!returned || !dropped || returned==dropped || !valid_inventory(inventory) || !click_valid(45,index,button,mode)) return false;
    inventory_view original; player_view(&original,inventory);
    if (view_alias(&original,returned) || view_alias(&original,dropped)) return false;
    mc_inventory working; mc_inventory_init(&working); mc_slot answer,drop; mc_slot_init(&answer); mc_slot_init(&drop);
    bool ok=mc_inventory_copy(&working,inventory); inventory_view view; player_view(&view,&working);
    if (ok) ok=execute_click(&view,index,button,mode,&answer,&drop) && valid_inventory(&working);
    if (ok) {
        mc_inventory_free(inventory); *inventory=working; mc_inventory_init(&working);
        mc_slot_free(returned); *returned=answer; mc_slot_init(&answer);
        mc_slot_free(dropped); *dropped=drop; mc_slot_init(&drop);
    }
    mc_inventory_free(&working); mc_slot_free(&answer); mc_slot_free(&drop); return ok;
}
bool mc_container_inventory_click_result(mc_inventory *player,mc_container *container,int index,int button,int mode,mc_slot *returned,mc_slot *dropped) {
    unsigned count=mc_container_slot_count(container);
    if (!returned || !dropped || returned==dropped || !valid_inventory(player) || !count || !click_valid(count,index,button,mode)) return false;
    inventory_view original; container_view(&original,player,container);
    if (view_alias(&original,returned) || view_alias(&original,dropped)) return false;
    for (unsigned i=0;i<45;i++) if (returned==&player->slots[i] || dropped==&player->slots[i]) return false;
    for (unsigned i=0;i<10;i++) if (returned==&container->slots[i] || dropped==&container->slots[i]) return false;
    mc_inventory working; mc_inventory_init(&working); mc_container window; mc_container_init(&window,container->kind);
    mc_slot answer,drop; mc_slot_init(&answer); mc_slot_init(&drop);
    bool ok=mc_inventory_copy(&working,player) && mc_container_copy(&window,container);
    inventory_view view; container_view(&view,&working,&window);
    if (ok) ok=execute_click(&view,index,button,mode,&answer,&drop) && valid_inventory(&working);
    if (ok) {
        mc_inventory_free(player); *player=working; mc_inventory_init(&working);
        mc_container_free(container); *container=window; mc_container_init(&window,window.kind);
        mc_slot_free(returned); *returned=answer; mc_slot_init(&answer);
        mc_slot_free(dropped); *dropped=drop; mc_slot_init(&drop);
    }
    mc_inventory_free(&working); mc_container_free(&window); mc_slot_free(&answer); mc_slot_free(&drop); return ok;
}
bool mc_inventory_click(mc_inventory *inventory,int index,int button,int mode,mc_slot *dropped) {
    mc_slot returned; mc_slot_init(&returned);
    bool accepted=mc_inventory_click_result(inventory,index,button,mode,&returned,dropped);
    mc_slot_free(&returned); return accepted;
}
