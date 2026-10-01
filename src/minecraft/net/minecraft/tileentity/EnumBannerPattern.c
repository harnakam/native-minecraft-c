#include "tileentity/EnumBannerPattern.h"
#include <string.h>
struct EnumBannerPatternRegistry { MCObject object; EnumBannerPatternArray *values; };
static void strings_trace(MCObject *o,MCObjectVisitor v,void *c) {BannerStringArray *a=(BannerStringArray *)o;for(int32_t i=0;i<a->length;i++)a->items[i]=(NBTString *)v((MCObject *)a->items[i],c);}
static void array_trace(MCObject *o,MCObjectVisitor v,void *c) {EnumBannerPatternArray *a=(EnumBannerPatternArray *)o;for(int32_t i=0;i<a->length;i++)a->items[i]=(EnumBannerPattern *)v((MCObject *)a->items[i],c);}
static void pattern_trace(MCObject *o,MCObjectVisitor v,void *c) {EnumBannerPattern *p=(EnumBannerPattern *)o;p->patternName=(NBTString *)v((MCObject *)p->patternName,c);p->patternID=(NBTString *)v((MCObject *)p->patternID,c);p->craftingLayers=(BannerStringArray *)v((MCObject *)p->craftingLayers,c);p->patternCraftingStack=(ItemStack *)v((MCObject *)p->patternCraftingStack,c);}
static void registry_trace(MCObject *o,MCObjectVisitor v,void *c) {EnumBannerPatternRegistry *r=(EnumBannerPatternRegistry *)o;r->values=(EnumBannerPatternArray *)v((MCObject *)r->values,c);}
static const MCObjectClass stringsClass={"native.EnumBannerPattern.StringArray",MCObjectHeap_plainClone,strings_trace,NULL};
static const MCObjectClass arrayClass={"native.EnumBannerPattern.Array",MCObjectHeap_plainClone,array_trace,NULL};
static const MCObjectClass patternClass={"TileEntityBanner.EnumBannerPattern",MCObjectHeap_plainClone,pattern_trace,NULL};
static const MCObjectClass registryClass={"native.EnumBannerPattern.StaticRegistry",MCObjectHeap_plainClone,registry_trace,NULL};
/* Original enum registration facts: identifiers, row-major nine-bit dye
   masks and six item/metadata templates. These are recipe/schema facts;
   gameplay arrays and ItemStacks are constructed as mutable managed refs. */
typedef struct {const char *name,*id;uint16_t mask;int16_t item,damage;} Fact;
static const Fact facts[]={
    {"base","b",0,-1,0},{"square_bottom_left","bl",64,-1,0},{"square_bottom_right","br",256,-1,0},
    {"square_top_left","tl",1,-1,0},{"square_top_right","tr",4,-1,0},{"stripe_bottom","bs",448,-1,0},
    {"stripe_top","ts",7,-1,0},{"stripe_left","ls",73,-1,0},{"stripe_right","rs",292,-1,0},
    {"stripe_center","cs",146,-1,0},{"stripe_middle","ms",56,-1,0},{"stripe_downright","drs",273,-1,0},
    {"stripe_downleft","dls",84,-1,0},{"small_stripes","ss",45,-1,0},{"cross","cr",341,-1,0},
    {"straight_cross","sc",186,-1,0},{"triangle_bottom","bt",336,-1,0},{"triangle_top","tt",21,-1,0},
    {"triangles_bottom","bts",168,-1,0},{"triangles_top","tts",42,-1,0},{"diagonal_left","ld",11,-1,0},
    {"diagonal_up_right","rd",416,-1,0},{"diagonal_up_left","lud",200,-1,0},{"diagonal_right","rud",38,-1,0},
    {"circle","mc",16,-1,0},{"rhombus","mr",170,-1,0},{"half_vertical","vh",219,-1,0},
    {"half_horizontal","hh",63,-1,0},{"half_vertical_right","vhr",438,-1,0},{"half_horizontal_bottom","hhb",504,-1,0},
    {"border","bo",495,-1,0},{"curly_border","cbo",0,106,0},{"creeper","cre",0,397,4},
    {"gradient","gra",149,-1,0},{"gradient_up","gru",338,-1,0},{"bricks","bri",0,45,0},
    {"skull","sku",0,397,1},{"flower","flo",0,38,8},{"mojang","moj",0,322,1}
};
static EnumBannerPatternArray *array_new(MCObjectHeap *h,int32_t n) {EnumBannerPatternArray *a=(EnumBannerPatternArray *)MCObjectHeap_alloc(h,sizeof(*a)+(size_t)n*sizeof(EnumBannerPattern *),&arrayClass);if(a)a->length=n;return a;}
static EnumBannerPattern *construct(MCObjectHeap *h,const Fact *f) {
    EnumBannerPattern *p=(EnumBannerPattern *)MCObjectHeap_alloc(h,sizeof(*p),&patternClass);if(!p)return NULL;
    p->craftingLayers=(BannerStringArray *)MCObjectHeap_alloc(h,sizeof(BannerStringArray)+3*sizeof(NBTString *),&stringsClass);if(!p->craftingLayers)return NULL;p->craftingLayers->length=3;
    p->patternName=NBTString_literalASCII(h,f->name);p->patternID=NBTString_literalASCII(h,f->id);if(!p->patternName||!p->patternID)return NULL;
    if(f->item>=0) {p->patternCraftingStack=ItemStack_new(h,ItemStack_registryItem(f->item),1,f->damage);if(!p->patternCraftingStack)return NULL;}
    else if(f->mask) for(int row=0;row<3;row++) {char chars[4]={' ',' ',' ',0};for(int col=0;col<3;col++)if(f->mask&(1u<<(row*3+col)))chars[col]='#';p->craftingLayers->items[row]=NBTString_literalASCII(h,chars);if(!p->craftingLayers->items[row])return NULL;}
    MCObjectHeap_touch(h);return p;
}
static bool registry_match(const MCObject *o,void *c) {(void)o;(void)c;return true;}
EnumBannerPatternRegistry *EnumBannerPattern_nativeRegistry(MCObjectHeap *h) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    EnumBannerPatternRegistry *r=(EnumBannerPatternRegistry *)MCObjectHeap_findObject(h,&registryClass,registry_match,NULL);
    if(!r) {
        r=(EnumBannerPatternRegistry *)MCObjectHeap_alloc(h,sizeof(*r),&registryClass);
        if(r) {r->values=array_new(h,(int32_t)(sizeof(facts)/sizeof(*facts)));if(!r->values)r=NULL;}
        for(int32_t i=0;r&&i<r->values->length;i++) {r->values->items[i]=construct(h,&facts[i]);if(!r->values->items[i])r=NULL;}
        /* The native root models a Java class static for this heap's lifetime.
           The heap owns its root entry; its ID is included in graph clones. */
        MCObjectRoot staticRoot={0};if(r&&!MCObjectRoot_init(&staticRoot,h,(MCObject *)r))r=NULL;
    }
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:r;
}
EnumBannerPatternArray *EnumBannerPattern_values(MCObjectHeap *h) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    EnumBannerPatternRegistry *r=EnumBannerPattern_nativeRegistry(h);EnumBannerPatternArray *out=r?array_new(h,r->values->length):NULL;
    if(out)memcpy(out->items,r->values->items,(size_t)out->length*sizeof(*out->items));
    MCObjectRootScope_end(&scope);return out;
}
NBTString *EnumBannerPattern_getPatternName(EnumBannerPattern *p) {return p->patternName;}
NBTString *EnumBannerPattern_getPatternID(EnumBannerPattern *p) {return p->patternID;}
BannerStringArray *EnumBannerPattern_getCraftingLayers(EnumBannerPattern *p) {return p->craftingLayers;}
bool EnumBannerPattern_hasValidCrafting(const EnumBannerPattern *p) {return p->patternCraftingStack||p->craftingLayers->items[0];}
bool EnumBannerPattern_hasCraftingStack(const EnumBannerPattern *p) {return p->patternCraftingStack!=NULL;}
ItemStack *EnumBannerPattern_getCraftingStack(EnumBannerPattern *p) {return p->patternCraftingStack;}
EnumBannerPattern *EnumBannerPattern_getPatternByID(MCObjectHeap *h,const NBTString *id) {
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    EnumBannerPattern *out=NULL;
    if(MCObjectRootScope_pin(&scope,(MCObject *)id)) {EnumBannerPatternArray *a=EnumBannerPattern_values(h);for(int32_t i=0;a&&i<a->length;i++)if(NBTString_equals(a->items[i]->patternID,id)){out=a->items[i];break;}}
    MCObjectRootScope_end(&scope);return out;
}
