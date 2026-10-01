#include "world/storage/MapStorage.h"
#include "world/storage/SaveDataMemoryStorage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"MapStorage check%u line%d: %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static NBTString *text(MCObjectHeap *h,const char *s){NBTString *v=NBTString_fromASCII(h,s);CHECK(v);return v;}
static int16_t last(MapStorage *s,NBTString *key){bool present=false;int16_t value=0;CHECK(MapStorage_nativeFindExactShort(s,key,&present,&value)&&present);return value;}
static void basic(void) {
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);CHECK(h);MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));
    MapStorage *s=MapStorage_nativeNewCounterProvider(h,NULL,NULL,NULL);CHECK(s);MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)s));
    NBTString *key=text(h,"map");int32_t id=-9;CHECK(MapStorage_getUniqueDataId(s,key,&id)&&id==0);
    CHECK(MapStorage_getUniqueDataId(s,key,&id)&&id==1);CHECK(last(s,key)==1);
    NBTString *equal=text(h,"map");CHECK(equal!=key&&MapStorage_getUniqueDataId(s,equal,&id)&&id==2);
    NBTString *entry=NULL;int16_t value=-3;CHECK(MapStorage_nativeIdCountEntryAt(s,0,&entry,&value)&&entry==key&&value==2);
    CHECK(MapStorage_getUniqueDataId(s,NULL,&id)&&id==0);CHECK(MapStorage_getUniqueDataId(s,NULL,&id)&&id==1);
    CHECK(last(s,NULL)==1);CHECK(MapStorage_getUniqueDataId(s,text(h,"other"),&id)&&id==0);
    const uint16_t units[]={0x65e5,0,0xd800,0x672c};NBTString *unicode=NBTString_fromUTF16(h,units,4);CHECK(unicode);
    CHECK(MapStorage_getUniqueDataId(s,unicode,&id)&&id==0);CHECK(MapStorage_getUniqueDataId(s,unicode,&id)&&id==1);
    CHECK(MapStorage_nativeIdCountSize(s)==4);NBTTagCompound *snapshot=MapStorage_nativeSnapshotIdCounts(s);CHECK(snapshot);
    CHECK(NBTTagCompound_getTagId(snapshot,NULL)==2&&NBTTagCompound_getShort(snapshot,unicode)==1);
    CHECK(MapStorage_nativeImportExactShort(s,key,32766));
    for(int32_t expected=32767;expected<=32769;expected++){uint16_t bits=(uint16_t)expected;int16_t signedValue;memcpy(&signedValue,&bits,2);CHECK(MapStorage_getUniqueDataId(s,key,&id)&&id==signedValue);}
    CHECK(MapStorage_nativeImportExactShort(s,key,-1)&&MapStorage_getUniqueDataId(s,key,&id)&&id==0);
    for(uint32_t i=1;i<=65536;i++){CHECK(MapStorage_getUniqueDataId(s,key,&id));CHECK((uint16_t)id==(uint16_t)i);}
    int32_t projected=-1;CHECK(MapStorage_nativeGetMapNextProjection(s,&projected)&&projected==1);
    CHECK(MapStorage_nativeClearIdCounts(s)&&MapStorage_nativeIdCountSize(s)==0);
    CHECK(MapStorage_nativeGetMapNextProjection(s,&projected)&&projected==0);
    CHECK(MapStorage_nativeImportIdCounts(s,snapshot,true)&&MapStorage_nativeIdCountSize(s)==4&&last(s,key)==2);
    CHECK(NBTTagCompound_setInteger_ascii(snapshot,"wrong",7)&&NBTTagCompound_setByte_ascii(snapshot,"byte",8));
    CHECK(MapStorage_nativeImportIdCounts(s,snapshot,true)&&MapStorage_nativeIdCountSize(s)==4);
    SaveDataMemoryStorage *memory=SaveDataMemoryStorage_nativeNewCounterProvider(h);CHECK(memory);
    CHECK(MapStorage_isInstance((MCObject *)memory)&&SaveDataMemoryStorage_isInstance((MCObject *)memory));
    CHECK(MapStorage_nativeImportExactShort(&memory->base,key,123));
    for(int i=0;i<100;i++){id=-4;CHECK(MapStorage_getUniqueDataId(&memory->base,i%2?key:NULL,&id)&&id==0);}
    CHECK(MapStorage_nativeIdCountSize(&memory->base)==1&&last(&memory->base,key)==123);
    memory->base.idCounts=NULL;CHECK(MapStorage_getUniqueDataId(&memory->base,NULL,&id)&&id==0); /* Override never reads it. */
    MCObjectRootScope_end(&scope);CHECK(MCObjectHeap_collect(h));CHECK(!MCObjectHeap_failed(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
}
typedef struct {
    MCObject object;MapStorage *storage;MCObject *file,*stream;NBTTagCompound *snapshot;NBTString *map;
    char events[32];unsigned eventsLength;int stage,exceptionAt,failureAt,prints,closes;
    bool missingFile,nullKeyWrite,mutateGetFile,mutateWrite,foreignFile,foreignStream,printFails,nullStream,invalidResult;
    int32_t observed;uint8_t bytes[4096];size_t size;
} IO;
static void io_trace(MCObject *o,MCObjectVisitor v,void *c){IO *i=(IO *)o;i->storage=(MapStorage *)v((MCObject *)i->storage,c);i->file=v(i->file,c);i->stream=v(i->stream,c);i->snapshot=(NBTTagCompound *)v((MCObject *)i->snapshot,c);i->map=(NBTString *)v((MCObject *)i->map,c);}
static const MCObjectClass ioClass={"MapStorageTestIO",MCObjectHeap_plainClone,io_trace,NULL};
static const MCObjectClass fileClass={"MapStorageTestFile",MCObjectHeap_plainClone,NULL,NULL};
static MCObject *external;
static MapStorageIOResult event(IO *io,int stage,char letter) {
    CHECK(io&&io->object.klass==&ioClass);io->stage=stage;CHECK(io->eventsLength+1<sizeof io->events);
    io->events[io->eventsLength++]=letter;io->events[io->eventsLength]=0;MCObjectHeap_touch(io->object.heap);
    CHECK(!MCObjectHeap_collect(io->object.heap)&&!MCObjectHeap_failed(io->object.heap));
    return io->invalidResult?(MapStorageIOResult)999:io->failureAt==stage?MAP_STORAGE_IO_FAILURE:io->exceptionAt==stage?MAP_STORAGE_IO_EXCEPTION:MAP_STORAGE_IO_OK;
}
static MapStorageIOResult file_name(MCObject *o,MCObject *save,NBTString *name,MCObject **out){
    IO *io=(IO *)o;CHECK(save==io->file&&NBTString_equalsASCII(name,"idcounts"));
    bool found=false;int16_t value=-1;CHECK(MapStorage_nativeFindExactShort(io->storage,io->map,&found,&value));io->observed=found?value:-999;
    MapStorageIOResult r=event(io,1,'F');if(r!=MAP_STORAGE_IO_OK)return r;
    if(io->mutateGetFile)CHECK(MapStorage_nativeImportExactShort(io->storage,io->map,42));
    *out=io->foreignFile?external:io->missingFile?NULL:io->file;return r;
}
static MapStorageIOResult open_output(MCObject *o,MCObject *file,MCObject **out){IO *io=(IO *)o;CHECK(file==io->file);MapStorageIOResult r=event(io,2,'O');if(r==MAP_STORAGE_IO_OK)*out=io->foreignStream?external:io->nullStream?NULL:io->stream;return r;}
static MapStorageIOResult write_nbt(MCObject *o,NBTTagCompound *tag,MCObject *stream){
    IO *io=(IO *)o;CHECK(stream==io->stream);io->snapshot=tag;MapStorageIOResult r=event(io,3,'W');if(r!=MAP_STORAGE_IO_OK)return r;
    NBTCompoundKeySet *keys=NBTTagCompound_getKeySet(tag);CHECK(keys);
    if(NBTCompoundKeySet_contains(keys,NULL)){io->nullKeyWrite=true;return MAP_STORAGE_IO_EXCEPTION;} /* Native DataOutput.writeUTF(NULL) dependency. */
    mc_buf bytes;mc_buf_init(&bytes);CHECK(NBTWire_encodeCompound(&bytes,tag));CHECK(bytes.len<=sizeof io->bytes);
    memcpy(io->bytes,bytes.data,bytes.len);io->size=bytes.len;mc_buf_free(&bytes);
    if(io->mutateWrite)CHECK(MapStorage_nativeImportExactShort(io->storage,io->map,999));
    return MAP_STORAGE_IO_OK;
}
static MapStorageIOResult close_output(MCObject *o,MCObject *stream){IO *io=(IO *)o;CHECK(stream==io->stream);++io->closes;return event(io,4,'C');}
static bool print_exception(MCObject *o){IO *io=(IO *)o;++io->prints;CHECK(io->eventsLength+1<sizeof io->events);io->events[io->eventsLength++]='P';io->events[io->eventsLength]=0;MCObjectHeap_touch(o->heap);return !io->printFails;}
static const MapStorageDependencies deps={file_name,open_output,write_nbt,close_output,print_exception};
static IO *io_fixture(MCObjectHeap *h,MCObjectRoot *root){
    IO *io=(IO *)MCObjectHeap_alloc(h,sizeof(*io),&ioClass);CHECK(io);
    io->file=MCObjectHeap_alloc(h,sizeof(MCObject),&fileClass);io->stream=MCObjectHeap_alloc(h,sizeof(MCObject),&fileClass);CHECK(io->file&&io->stream);
    io->map=text(h,"map");io->storage=MapStorage_nativeNewCounterProvider(h,io->file,(MCObject *)io,&deps);CHECK(io->storage);
    CHECK(MCObjectRoot_init(root,h,(MCObject *)io->storage));return io;
}
static void io_paths(void){
    for(int exception=0;exception<=4;exception++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024);MCObjectRoot root={0};IO *io=io_fixture(h,&root);io->exceptionAt=exception;int32_t value=99;
        CHECK(MapStorage_getUniqueDataId(io->storage,io->map,&value)&&value==0&&io->observed==0);
        const char *expected[]={"FOWC","FP","FOP","FOWP","FOWCP"};CHECK(strcmp(io->events,expected[exception])==0);
        CHECK(io->prints==(exception!=0)&&io->closes==(exception==0||exception==4)&&last(io->storage,io->map)==0);
        CHECK(!MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));
        if(!exception){CHECK(io->size>3&&io->bytes[0]==10&&io->bytes[1]==0&&io->bytes[2]==0);mc_buf b;mc_buf_init(&b);mc_put_bytes(&b,io->bytes,io->size);CHECK(!b.failed);NBTSizeTracker tracker;NBTSizeTracker_init(&tracker,2097152);NBTTagCompound *decoded=NULL;CHECK(NBTWire_decodeCompound(h,&b,&tracker,&decoded));CHECK(NBTTagCompound_getTagId_ascii(decoded,"map")==2&&NBTTagCompound_getShort_ascii(decoded,"map")==0);mc_buf_free(&b);}
        MCObjectRoot_drop(&root);MCObjectHeap_free(h);
    }
    for(int stage=1;stage<=4;stage++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024);MCObjectRoot root={0};IO *io=io_fixture(h,&root);io->failureAt=stage;int32_t value=99;
        CHECK(!MapStorage_getUniqueDataId(io->storage,io->map,&value)&&value==99&&io->observed==0&&io->stage==stage);
        CHECK(MCObjectHeap_failed(h)&&io->prints==0&&!MCObjectHeap_hasBorrowers(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
    }
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);MCObjectRoot root={0};IO *io=io_fixture(h,&root);int32_t value=-1;
    io->missingFile=true;CHECK(MapStorage_getUniqueDataId(io->storage,io->map,&value)&&value==0&&strcmp(io->events,"F")==0&&!io->snapshot);
    io->missingFile=false;io->eventsLength=0;io->mutateGetFile=true;io->mutateWrite=true;
    CHECK(MapStorage_getUniqueDataId(io->storage,io->map,&value)&&value==1&&last(io->storage,io->map)==999);
    CHECK(io->snapshot&&NBTTagCompound_getShort(io->snapshot,io->map)==42&&strcmp(io->events,"FOWC")==0);
    io->eventsLength=0;io->mutateGetFile=false;io->mutateWrite=false;
    CHECK(MapStorage_getUniqueDataId(io->storage,NULL,&value)&&value==0&&io->nullKeyWrite&&io->prints==1&&strcmp(io->events,"FOWP")==0);
    CHECK(last(io->storage,NULL)==0&&!MCObjectHeap_failed(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
}
static void graph(void){
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);MCObjectRoot root={0};IO *io=io_fixture(h,&root);io->missingFile=true;int32_t id=-1;
    CHECK(MapStorage_getUniqueDataId(io->storage,io->map,&id)&&id==0);CHECK(MCObjectHeap_collect(h));
    size_t baseline=MCObjectHeap_liveObjects(h);MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);MCObjectRoot branch={0};CHECK(MCObjectRoot_rebind(&branch,copy,&root));
    MapStorage *s=(MapStorage *)MCObjectRoot_get(&branch);IO *work=(IO *)s->nativeContext;
    CHECK(work!=io&&work->storage==s&&s->saveHandler==work->file&&work->map!=io->map);
    CHECK(MapStorage_getUniqueDataId(s,work->map,&id)&&id==1&&last(io->storage,io->map)==0);
    int32_t projection=-1;CHECK(MapStorage_nativeGetMapNextProjection(io->storage,&projection)&&projection==1);
    CHECK(MCObjectHeap_canAdopt(h,copy)&&MCObjectHeap_adopt(h,copy));MCObjectHeap_free(copy);
    s=(MapStorage *)MCObjectRoot_get(&root);io=(IO *)s->nativeContext;CHECK(last(s,io->map)==1&&io->storage==s);
    CHECK(MCObjectHeap_collect(h)&&MCObjectHeap_liveObjects(h)==baseline);
    copy=MCObjectHeap_clone(h);CHECK(copy&&MCObjectRoot_rebind(&branch,copy,&root));s=(MapStorage *)MCObjectRoot_get(&branch);work=(IO *)s->nativeContext;
    work->missingFile=false;work->failureAt=3;CHECK(!MapStorage_getUniqueDataId(s,work->map,&id)&&MCObjectHeap_failed(copy));
    CHECK(!MCObjectHeap_canAdopt(h,copy));MCObjectHeap_free(copy);s=(MapStorage *)MCObjectRoot_get(&root);CHECK(last(s,io->map)==1);
    CHECK(MCObjectHeap_collect(h)&&!MCObjectHeap_failed(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
}
static void failures(void){
    MCObjectHeap *other=MCObjectHeap_new(1024);external=MCObjectHeap_alloc(other,sizeof(MCObject),&fileClass);CHECK(external);
    for(int stream=0;stream<2;stream++){MCObjectHeap *h=MCObjectHeap_new(1024*1024);MCObjectRoot root={0};IO *io=io_fixture(h,&root);io->foreignFile=!stream;io->foreignStream=stream;int32_t id=77;CHECK(!MapStorage_getUniqueDataId(io->storage,io->map,&id)&&id==77&&MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);}
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);MapStorage *s=MapStorage_nativeNewCounterProvider(h,external,NULL,NULL);CHECK(!s&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024);s=MapStorage_nativeNewCounterProvider(h,NULL,NULL,NULL);CHECK(s);NBTString *foreign=text(other,"map");int32_t id=8;CHECK(!MapStorage_getUniqueDataId(s,foreign,&id)&&id==8&&MCObjectHeap_failed(h));MCObjectHeap_free(h);MCObjectHeap_free(other);external=NULL;
    unsigned failed=0,succeeded=0;
    for(size_t budget=64;budget<8192;budget+=32){
        h=MCObjectHeap_new(budget);CHECK(h);s=MapStorage_nativeNewCounterProvider(h,NULL,NULL,NULL);
        NBTString *key=s?NBTString_fromASCII(h,"new"):NULL;id=99;
        if(key&&MapStorage_getUniqueDataId(s,key,&id)){CHECK(id==0);++succeeded;}else{CHECK(id==99&&MCObjectHeap_failed(h));++failed;}
        CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
    }
    CHECK(failed&&succeeded);
}
static void hash_order(void){
    MCObjectHeap *h=MCObjectHeap_new(1024*1024);MapStorage *s=MapStorage_nativeNewCounterProvider(h,NULL,NULL,NULL);CHECK(s);
    const char *names[]={"Aa","BB","map","x","a","n","k9","k0","k12","k11","k10","k1","k2","k3","k4","k5","k6","k7","k8"};
    for(size_t i=0;i<sizeof names/sizeof names[0];i++){int32_t id=-1;CHECK(MapStorage_getUniqueDataId(s,text(h,names[i]),&id)&&id==0);}
    /* Java8 HashMap bucket traversal, after the same original insertion list. */
    const char *order[]={"Aa","BB","a","k0","k1","k2","k3","k4","k11","k5","k10","k6","k7","k12","k8","n","k9","x","map"};
    for(int32_t i=0;i<19;i++){NBTString *key=NULL;int16_t value=-1;CHECK(MapStorage_nativeIdCountEntryAt(s,i,&key,&value)&&NBTString_equalsASCII(key,order[i])&&value==0);}
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(1024*1024);s=MapStorage_nativeNewCounterProvider(h,NULL,NULL,NULL);CHECK(s);
    for(unsigned n=0;n<11;n++){char key[9];for(unsigned i=0;i<4;i++){key[i*2]=(n&(1u<<i))?'B':'A';key[i*2+1]=(n&(1u<<i))?'B':'a';}key[8]=0;int32_t id=88;bool ok=MapStorage_getUniqueDataId(s,text(h,key),&id);if(n<10)CHECK(ok&&id==0);else CHECK(!ok&&id==88&&MCObjectHeap_failed(h));}
    CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectHeap_free(h);
}
static void native_dependency_failures(void){
    const char *expected[]={"","F","FO","FOW","F","FP","F","FO"};
    for(int branch=0;branch<8;branch++) {
        MCObjectHeap *h=MCObjectHeap_new(1024*1024);MCObjectRoot root={0};IO *io=io_fixture(h,&root);
        MapStorageDependencies missing=deps;io->storage->dependencies=&missing;
        if(branch==0)missing.getMapFileFromName=NULL;
        if(branch==1)missing.newDataOutputStream=NULL;
        if(branch==2)missing.writeRawNBT=NULL;
        if(branch==3)missing.closeOutputStream=NULL;
        if(branch==4){io->exceptionAt=1;missing.printCaughtException=NULL;}
        if(branch==5){io->exceptionAt=1;io->printFails=true;}
        if(branch==6)io->invalidResult=true;
        if(branch==7)io->nullStream=true;
        int32_t id=99;CHECK(!MapStorage_getUniqueDataId(io->storage,io->map,&id)&&id==99);
        CHECK(strcmp(io->events,expected[branch])==0&&MCObjectHeap_failed(h)&&!MCObjectHeap_hasBorrowers(h));
        MCObjectRoot_drop(&root);MCObjectHeap_free(h);
    }
    unsigned oom=0,ok=0;
    for(size_t spare=0;spare<=2048;spare+=32){
        const size_t budget=16384;MCObjectHeap *h=MCObjectHeap_new(budget);MCObjectRoot root={0};IO *io=io_fixture(h,&root);
        io->missingFile=true;int32_t id=99;CHECK(MapStorage_getUniqueDataId(io->storage,io->map,&id)&&id==0);
        io->missingFile=false;io->eventsLength=0;io->stage=0;io->events[0]=0;
        size_t live=MCObjectHeap_liveBytes(h);CHECK(budget>live+spare+sizeof(MCObject));
        CHECK(MCObjectHeap_alloc(h,budget-live-spare,&fileClass));
        id=99;bool success=MapStorage_getUniqueDataId(io->storage,io->map,&id);
        CHECK(io->observed==1); /* Source put precedes every allocation/IO failure. */
        if(success){CHECK(id==1&&strcmp(io->events,"FOWC")==0);++ok;}
        else {CHECK(id==99&&MCObjectHeap_failed(h)&&io->stage==1&&io->closes==0&&io->prints==0);++oom;}
        int32_t next=-1;CHECK(MapStorage_nativeGetMapNextProjectionDiagnostic(io->storage,&next)&&next==2);
        if(!success){next=-1;CHECK(!MapStorage_nativeGetMapNextProjection(io->storage,&next)&&next==-1&&MCObjectHeap_failed(h));}
        CHECK(!MCObjectHeap_hasBorrowers(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
    }
    CHECK(oom&&ok);
}
int main(void) {
    basic();io_paths();graph();failures();hash_order();native_dependency_failures();
    printf("Source MapStorage counter: %u checks passed\n",checks);return 0;
}
