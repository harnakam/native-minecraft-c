#include "util/NativeJavaClass.h"
#include "client/entity/EntityPlayerSP.h"
#include "entity/player/EntityPlayerMP.h"
#include <stdio.h>
#include <stdlib.h>

static int checks;
#define CHECK(test) do { ++checks; if(!(test)) {fprintf(stderr,"native Class check %d failed at %d: %s\n",checks,__LINE__,#test);exit(1);} } while(0)
static const MCObjectClass fixtureClass={"fixture.Diamond",MCObjectHeap_plainClone,NULL,NULL};
static bool diamond(const MCObject *o){return o&&o->klass==&fixtureClass;}
static const NativeJavaClassDescriptor base={"fixture.Base",NULL,0,NULL};
static const NativeJavaClassDescriptor *const leftParents[]={&base};
static const NativeJavaClassDescriptor left={"fixture.Left",leftParents,1,NULL};
static const NativeJavaClassDescriptor *const rightParents[]={&base};
static const NativeJavaClassDescriptor right={"fixture.Right",rightParents,1,NULL};
static const NativeJavaClassDescriptor *const diamondParents[]={&left,&right};
static const NativeJavaClassDescriptor child={"fixture.Diamond",diamondParents,2,diamond};
static const NativeJavaClassDescriptor unrelated={"fixture.Other",NULL,0,NULL};

static void canonical_and_hierarchy(void){
    MCObjectHeap *h=MCObjectHeap_new(1000000);CHECK(h);
    NativeJavaClass *c=NativeJavaClass_literal(h,&child),*b=NativeJavaClass_literal(h,&base),
        *l=NativeJavaClass_literal(h,&left),*r=NativeJavaClass_literal(h,&right),
        *other=NativeJavaClass_literal(h,&unrelated);
    CHECK(c&&b&&l&&r&&other);CHECK(NativeJavaClass_literal(h,&child)==c);
    CHECK(NativeJavaClass_isInstance((MCObject *)c));
    MCObject *v=MCObjectHeap_alloc(h,sizeof(MCObject),&fixtureClass);CHECK(v);
    CHECK(NativeJavaClass_getClass(h,v)==c);
    bool out=false;
    CHECK(NativeJavaClass_isAssignableFrom(c,c,&out)&&out);
    CHECK(NativeJavaClass_isAssignableFrom(b,c,&out)&&out);
    CHECK(NativeJavaClass_isAssignableFrom(l,c,&out)&&out);
    CHECK(NativeJavaClass_isAssignableFrom(r,c,&out)&&out);
    CHECK(NativeJavaClass_isAssignableFrom(c,b,&out)&&!out);
    CHECK(NativeJavaClass_isAssignableFrom(other,c,&out)&&!out);
    CHECK(NativeJavaClass_isInstanceOf(b,v,&out)&&out);
    CHECK(NativeJavaClass_isInstanceOf(b,NULL,&out)&&!out);
    MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,v));
    CHECK(MCObjectHeap_collect(h));CHECK(NativeJavaClass_literal(h,&child)==c);
    MCObjectHeap *copy=MCObjectHeap_clone(h);CHECK(copy);
    MCObjectRoot copied={0};CHECK(MCObjectRoot_rebind(&copied,copy,&root));
    NativeJavaClass *cc=NativeJavaClass_literal(copy,&child);CHECK(cc&&cc!=c);
    CHECK(NativeJavaClass_getClass(copy,MCObjectRoot_get(&copied))==cc);
    CHECK(MCObjectHeap_canAdopt(h,copy));CHECK(MCObjectHeap_adopt(h,copy));
    MCObjectHeap_free(copy);
    NativeJavaClass *adopted=NativeJavaClass_literal(h,&child);CHECK(adopted&&adopted!=c);
    CHECK(NativeJavaClass_getClass(h,MCObjectRoot_get(&root))==adopted);
    MCObjectRoot_drop(&root);CHECK(MCObjectHeap_collect(h));
    CHECK(NativeJavaClass_literal(h,&child)==adopted);CHECK(!MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
}
static void source_entity_runtime_classes(void){
    MCObjectHeap *h=MCObjectHeap_new(1000000);CHECK(h);
    EntityPlayerSP *sp=EntityPlayerSP_nativeAllocate(h);
    EntityPlayerMP *mp=EntityPlayerMP_nativeAllocate(h);CHECK(sp&&mp);
    NativeJavaClass *s=NativeJavaClass_getClass(h,(MCObject *)sp),
        *m=NativeJavaClass_getClass(h,(MCObject *)mp),*e=NativeJavaClass_Entity(h),
        *o=NativeJavaClass_Object(h);CHECK(s&&m&&e&&o&&s!=m);
    bool out=false;
    CHECK(NativeJavaClass_isAssignableFrom(e,s,&out)&&out);
    CHECK(NativeJavaClass_isAssignableFrom(e,m,&out)&&out);
    CHECK(NativeJavaClass_isAssignableFrom(o,e,&out)&&out);
    CHECK(NativeJavaClass_isAssignableFrom(s,m,&out)&&!out);
    CHECK(!MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
static void rejected_boundaries(void){
    MCObjectHeap *h=MCObjectHeap_new(100000);CHECK(h);
    MCObject *unknown=MCObjectHeap_alloc(h,sizeof(MCObject),&fixtureClass);CHECK(unknown);
    CHECK(!NativeJavaClass_getClass(h,unknown)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
    h=MCObjectHeap_new(100000);CHECK(h);
    NativeJavaClass *b=NativeJavaClass_literal(h,&base);CHECK(b);
    bool out=true;CHECK(!NativeJavaClass_isAssignableFrom(b,NULL,&out)&&out&&MCObjectHeap_failed(h));
    MCObjectHeap_free(h);
    h=MCObjectHeap_new(100000);MCObjectHeap *foreign=MCObjectHeap_new(100000);CHECK(h&&foreign);
    b=NativeJavaClass_literal(h,&base);NativeJavaClass *f=NativeJavaClass_literal(foreign,&child);CHECK(b&&f);
    out=true;CHECK(!NativeJavaClass_isAssignableFrom(b,f,&out)&&out&&MCObjectHeap_failed(h));
    CHECK(!MCObjectHeap_failed(foreign));MCObjectHeap_free(h);MCObjectHeap_free(foreign);
    h=MCObjectHeap_new(100000);CHECK(h);
    NativeJavaClassDescriptor bad={"fixture.Bad",NULL,1,NULL};
    CHECK(!NativeJavaClass_literal(h,&bad)&&MCObjectHeap_failed(h));MCObjectHeap_free(h);
}
int main(void){canonical_and_hierarchy();source_entity_runtime_classes();rejected_boundaries();
    printf("Native Class dependency: %d checks\n",checks);return 0;}
