#include "util/MCGameplay.h"
#include "entity/player/InventoryPlayer.h"
#include "nbt/NBTTagCompound.h"
#include "nbt/NBTSizeTracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#define remove_dir(path) _rmdir(path)
#else
#include <unistd.h>
#define make_dir(path) mkdir(path,0700)
#define remove_dir(path) rmdir(path)
#endif
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"gameplay graph: %s at %d\n",#x,__LINE__); exit(1); } } while (0)
typedef struct { MCObject object; InventoryPlayer *inventory; InventoryCrafting *grid; MCObject *world; } Actor;
static void trace_actor(MCObject *object,MCObjectVisitor visitor,void *context) {
    Actor *actor=(Actor *)object;
    actor->inventory=(InventoryPlayer *)visitor((MCObject *)actor->inventory,context);
    actor->grid=(InventoryCrafting *)visitor((MCObject *)actor->grid,context);
    actor->world=visitor(actor->world,context);
}
static const MCObjectClass actor_class={"test.GameActor",MCObjectHeap_plainClone,trace_actor,NULL};
static bool survival(const MCObject *actor) { (void)actor; return false; }
static bool changed(MCObject *actor,InventoryCrafting *grid) { return ((Actor *)actor)->grid==grid; }
static Actor *actor_new(MCGameplay *game) {
    Actor *actor=(Actor *)MCObjectHeap_alloc(game->heap,sizeof(*actor),&actor_class); CHECK(actor);
    actor->world=MCGameplay_get(game)->world;
    actor->inventory=InventoryPlayer_new(game->heap,(MCObject *)actor,survival); CHECK(actor->inventory);
    actor->grid=InventoryCrafting_new(game->heap,(MCObject *)actor,changed,2,2); CHECK(actor->grid);
    return actor;
}
static bool encode_tag(NBTTagCompound *tag,mc_nbt *output) {
    mc_buf buffer={0}; bool ok=NBTWire_encodeCompound(&buffer,tag);
    if (ok) { buffer.pos=0; ok=mc_nbt_read(&buffer,output) && buffer.pos==buffer.len; }
    mc_buf_free(&buffer); return ok;
}
typedef struct { int failPlayer; bool failItems; MCGameplay *mutateParent; unsigned players,items,maps; } Encode;
static bool encode_player(const MCGameplayObjects *objects,size_t index,mc_nbt *output,void *context) {
    Encode *encode=context; ++encode->players;
    if ((int)index==encode->failPlayer) return false;
    Actor *actor=(Actor *)objects->players[index]; MCObjectHeap *heap=objects->object.heap;
    NBTTagCompound *tag=NBTTagCompound_new(heap); NBTTagList *list=NBTTagList_new(heap); CHECK(tag&&list);
    if (!InventoryPlayer_writeToNBT(actor->inventory,list) || !NBTTagCompound_setTag_ascii(tag,"Inventory",(NBTBase *)list)) return false;
    ItemStack *stack=InventoryCrafting_getStackInSlot(actor->grid,0);
    if (stack) {
        NBTTagCompound *grid=NBTTagCompound_new(heap); CHECK(grid);
        if (!ItemStack_writeToNBT(stack,grid) || !NBTTagCompound_setTag_ascii(tag,"Grid",(NBTBase *)grid)) return false;
    }
    return encode_tag(tag,output);
}
static bool encode_items(const MCGameplayObjects *objects,mc_nbt *output,void *context) {
    Encode *encode=context; ++encode->items;
    if (encode->failItems) return false;
    NBTTagCompound *tag=NBTTagCompound_new(objects->object.heap); NBTTagList *list=NBTTagList_new(objects->object.heap); CHECK(tag&&list);
    for (size_t i=0;i<objects->itemCount;i++) {
        NBTTagCompound *entry=NBTTagCompound_new(objects->object.heap); CHECK(entry);
        if (!ItemStack_writeToNBT((ItemStack *)objects->items[i],entry) || !NBTTagList_appendTag(list,(NBTBase *)entry)) return false;
    }
    if (encode->mutateParent) MCObjectHeap_touch(encode->mutateParent->heap);
    return NBTTagCompound_setTag_ascii(tag,"Items",(NBTBase *)list) && encode_tag(tag,output);
}
static bool encode_maps(const MCGameplayObjects *objects,mc_nbt *output,void *context) {
    ++((Encode *)context)->maps; return encode_tag((NBTTagCompound *)objects->world,output);
}
static const MCGameplayEncoders encoders={encode_player,encode_items,encode_maps};
static const char *uuids[]={"00000000-0000-3000-8000-000000000001","00000000-0000-3000-8000-000000000002"};
static const char *base="test-gameplay-graph.c919";
static void player_path(char path[256],size_t index) { snprintf(path,256,"%s.players/%s.dat",base,uuids[index]); }
static int stored_grid_count(const char *path) {
    mc_nbt nbt={0}; char error[256]; CHECK(mc_nbt_load_gzip(&nbt,path,error,sizeof error));
    mc_nbt_view root,grid,count; int64_t value;
    CHECK(mc_nbt_root(&nbt,&root)&&mc_nbt_find(&root,"Grid",&grid)&&mc_nbt_find(&grid,"Count",&count)&&mc_nbt_get_integer(&count,&value));
    mc_nbt_free(&nbt); return (int)value;
}
static void setup(MCGameplay *game) {
    CHECK(MCGameplay_init(game,16*1024*1024)); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,game->heap));
    NBTTagCompound *world=NBTTagCompound_new(game->heap); CHECK(world); CHECK(MCGameplay_setWorld(game,(MCObject *)world));
    Actor *first=actor_new(game),*second=actor_new(game);
    CHECK(MCGameplay_setPlayer(game,0,uuids[0],(MCObject *)first)); CHECK(MCGameplay_setPlayer(game,1,uuids[1],(MCObject *)second));
    CHECK(MCGameplay_setPlayer(game,0,MCGameplay_get(game)->uuids[0],(MCObject *)first));
    CHECK(!strcmp(MCGameplay_get(game)->uuids[0],uuids[0]));
    ItemStack *book=ItemStack_new(game->heap,ItemStack_registryItem(387),1,7); CHECK(book);
    NBTTagCompound *tag=NBTTagCompound_new(game->heap); CHECK(tag); CHECK(NBTTagCompound_setInteger_ascii(tag,"generation",0)); CHECK(ItemStack_setTagCompound(book,tag));
    CHECK(InventoryCrafting_setInventorySlotContents(first->grid,0,book)); CHECK(MCGameplay_addItem(game,(MCObject *)book));
    CHECK(!MCGameplay_addItem(game,(MCObject *)book)); CHECK(!MCGameplay_setPlayer(game,2,uuids[0],(MCObject *)second));
    CHECK(!MCGameplay_setPlayer(game,2,"../../secret",(MCObject *)second)); CHECK(!MCGameplay_setPlayer(game,64,uuids[0],(MCObject *)second));
    MCObjectRootScope_end(&scope);
}
static void pickup(MCGameplayTransaction *transaction) {
    MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,transaction->working.heap));
    MCGameplayObjects *objects=MCGameplay_get(&transaction->working);
    Actor *first=(Actor *)objects->players[0],*second=(Actor *)objects->players[1]; ItemStack *book=(ItemStack *)objects->items[0];
    CHECK(InventoryCrafting_getStackInSlot(first->grid,0)==book);
    CHECK(InventoryPlayer_addItemStackToInventory(second->inventory,book)); CHECK(book->stackSize==0);
    CHECK(InventoryCrafting_getStackInSlot(first->grid,0)==book);
    CHECK(InventoryPlayer_getStackInSlot(second->inventory,0)!=book);
    MCObjectRootScope_end(&scope);
}
static void no_commit_failures(MCGameplay *game) {
    Encode encode={-1,false,NULL,0,0,0}; char error[256]; MCGameplayTransaction transaction={0};
    CHECK(MCGameplay_begin(game,&transaction)); pickup(&transaction); encode.failPlayer=1;
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_NOT_COMMITTED);
    CHECK(!transaction.active && ((ItemStack *)MCGameplay_get(game)->items[0])->stackSize==1); CHECK(!game->fatal);
    encode=(Encode){-1,true,NULL,0,0,0}; CHECK(MCGameplay_begin(game,&transaction)); pickup(&transaction);
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_NOT_COMMITTED);
    CHECK(((ItemStack *)MCGameplay_get(game)->items[0])->stackSize==1);
    encode=(Encode){-1,false,game,0,0,0}; CHECK(MCGameplay_begin(game,&transaction)); pickup(&transaction);
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_NOT_COMMITTED);
    CHECK(((ItemStack *)MCGameplay_get(game)->items[0])->stackSize==1);
    CHECK(MCGameplay_begin(game,&transaction)); MCObjectHeap_touch(game->heap); encode=(Encode){-1,false,NULL,0,0,0};
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_NOT_COMMITTED); CHECK(encode.players==0);
    CHECK(MCGameplay_begin(game,&transaction)); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,transaction.working.heap));
    CHECK(!MCGameplay_abort(&transaction)); CHECK(transaction.active);
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_NOT_COMMITTED); CHECK(transaction.active);
    MCObjectRootScope_end(&scope); CHECK(MCGameplay_abort(&transaction));
    CHECK(MCGameplay_begin(game,&transaction)); MCGameplayTransaction nested={0};
    CHECK(!MCGameplay_begin(&transaction.working,&nested)); CHECK(!nested.active);
    CHECK(MCGameplay_abort(&transaction));
    CHECK(MCGameplay_begin(game,&transaction)); transaction.working.fatal=true;
    encode=(Encode){-1,false,NULL,0,0,0};
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_NOT_COMMITTED);
    CHECK(!transaction.active && !game->fatal && encode.players==0);
    CHECK(MCGameplay_begin(game,&transaction)); CHECK(MCObjectRoot_set(&transaction.working.root,NULL));
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_NOT_COMMITTED);
    CHECK(!transaction.active && !game->fatal);
    struct stat info; CHECK(stat("test-gameplay-graph.c919.transfer.dat",&info)!=0);
}
int main(void) {
    char path[256],error[256]; MCGameplay game={0}; setup(&game);
    CHECK(make_dir("test-gameplay-graph.c919.players")==0);
    no_commit_failures(&game);
    MCGameplayTransaction transaction={0}; Encode encode={-1,false,NULL,0,0,0}; CHECK(MCGameplay_begin(&game,&transaction)); pickup(&transaction);
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_COMMITTED);
    CHECK(encode.players==2&&encode.items==1&&encode.maps==1); CHECK(!transaction.active&&!game.fatal);
    Actor *first=(Actor *)MCGameplay_get(&game)->players[0];
    CHECK(InventoryCrafting_getStackInSlot(first->grid,0)==(ItemStack *)MCGameplay_get(&game)->items[0]);
    CHECK(InventoryCrafting_getStackInSlot(first->grid,0)->stackSize==0); CHECK(MCObjectHeap_collect(game.heap));
    player_path(path,0); CHECK(stored_grid_count(path)==0);
    /* A checkpoint error after durable commitment adopts every owner and
       prevents another transaction until restart recovery has finished. */
    CHECK(MCGameplay_begin(&game,&transaction)); MCObjectRootScope scope={0}; CHECK(MCObjectRootScope_begin(&scope,transaction.working.heap));
    ((ItemStack *)MCGameplay_get(&transaction.working)->items[0])->stackSize=5; MCObjectRootScope_end(&scope);
    player_path(path,1); CHECK(remove(path)==0); CHECK(make_dir(path)==0);
    encode=(Encode){-1,false,NULL,0,0,0};
    CHECK(MCGameplay_commit(&transaction,base,&encoders,&encode,error,sizeof error)==MC_GAMEPLAY_COMMITTED_NEEDS_RECOVERY);
    CHECK(game.fatal&&!transaction.active); CHECK(!MCGameplay_begin(&game,&transaction));
    MCGameplayObjects *objects=(MCGameplayObjects *)MCObjectRoot_get(&game.root); CHECK(((ItemStack *)objects->items[0])->stackSize==5);
    first=(Actor *)objects->players[0]; CHECK(InventoryCrafting_getStackInSlot(first->grid,0)==(ItemStack *)objects->items[0]);
    CHECK(remove_dir(path)==0); CHECK(mc_transfer_recover(base,error,sizeof error));
    player_path(path,0); CHECK(stored_grid_count(path)==5);
    for (size_t i=0;i<2;i++) { player_path(path,i); CHECK(remove(path)==0); }
    CHECK(remove("test-gameplay-graph.c919.items.dat")==0); CHECK(remove("test-gameplay-graph.c919.maps.dat")==0);
    CHECK(remove_dir("test-gameplay-graph.c919.players")==0); CHECK(MCGameplay_free(&game));
    printf("gameplay graph: %u checks passed\n",checks); return 0;
}
