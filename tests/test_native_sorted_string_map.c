#include "util/NativeSortedStringMap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Sorted map check%u line%d %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static int compare(NBTString *a,NBTString *b){size_t n=NBTString_length(a)<NBTString_length(b)?NBTString_length(a):NBTString_length(b);for(size_t i=0;i<n;i++){uint16_t x=NBTString_units(a)[i],y=NBTString_units(b)[i];if(x!=y)return x>y?1:-1;}return NBTString_length(a)>NBTString_length(b)?1:NBTString_length(a)<NBTString_length(b)?-1:0;}
static unsigned validate(NativeSortedStringEntry *p,NativeSortedStringEntry *parent,size_t *n){if(!p)return 1;CHECK(p->parent==parent);if(p->left)CHECK(compare(p->left->key,p->key)<0);if(p->right)CHECK(compare(p->right->key,p->key)>0);if(p->red)CHECK((!p->left||!p->left->red)&&(!p->right||!p->right->red));(*n)++;unsigned a=validate(p->left,p,n),b=validate(p->right,p,n);CHECK(a==b);return a+(p->red?0u:1u);}
static void invariant(NativeSortedStringMap *m){CHECK(!m->root||!m->root->red);size_t n=0;validate(m->root,NULL,&n);CHECK(n==(size_t)m->size);}
int main(void){
 MCObjectHeap *h=MCObjectHeap_new(2u*1024u*1024u);CHECK(h);NativeSortedStringMap *m=NativeSortedStringMap_new(h);CHECK(m);MCObjectRoot root={0};CHECK(MCObjectRoot_init(&root,h,(MCObject *)m));
 NBTString *first=NULL;for(unsigned i=0;i<1000;i++){unsigned id=(i*719u)%1000;char key[16];snprintf(key,sizeof key,"k%04u",id);NBTString *s=NBTString_fromASCII(h,key);CHECK(s&&NativeSortedStringMap_put(m,s,(MCObject *)s));if(!id)first=s;invariant(m);}
 CHECK(m->size==1000&&m->modCount==1000);NBTString *equal=NBTString_fromASCII(h,"k0000");CHECK(equal&&equal!=first);CHECK(NativeSortedStringMap_put(m,equal,(MCObject *)m));NBTString *k;MCObject *v;CHECK(NativeSortedStringMap_entryAt(m,0,&k,&v)&&k==first&&v==(MCObject *)m&&m->modCount==1000);
 uint16_t custom[][3]={{0,'a',0},{0,'b',0},{0xd800,0,0},{0xdc00,0,0},{0xffff,0,0}};for(size_t i=0;i<5;i++){NBTString *s=NBTString_fromUTF16(h,custom[i],2);CHECK(s&&NativeSortedStringMap_put(m,s,NULL)&&NativeSortedStringMap_containsKey(m,s));}invariant(m);
 MCObjectRootScope scope={0};CHECK(MCObjectRootScope_begin(&scope,h));NativeSortedStringCursor cursor;CHECK(NativeSortedStringMap_beginCursor(m,&cursor));NBTString *previous=NULL;int32_t size=0;while(cursor.next){CHECK(NativeSortedStringMap_next(&cursor,&k,&v));if(previous)CHECK(compare(previous,k)<0);previous=k;size++;}CHECK(size==1005);MCObjectRootScope_end(&scope);
 CHECK(MCObjectHeap_collect(h));m=(NativeSortedStringMap *)MCObjectRoot_get(&root);CHECK(m->size==1005);CHECK(NativeSortedStringMap_entryAt(m,2,&k,&v)&&v==(MCObject *)m);MCObjectHeap *branch=MCObjectHeap_clone(h);CHECK(branch);MCObjectRoot br={0};CHECK(MCObjectRoot_rebind(&br,branch,&root));NativeSortedStringMap *copy=(NativeSortedStringMap *)MCObjectRoot_get(&br);CHECK(copy!=m&&copy->root!=m->root);invariant(copy);CHECK(NativeSortedStringMap_entryAt(copy,2,&k,&v)&&v==(MCObject *)copy);CHECK(NativeSortedStringMap_put(copy,NBTString_fromASCII(branch,"new"),NULL));CHECK(m->size==1005&&copy->size==1006);CHECK(MCObjectHeap_adopt(h,branch));MCObjectHeap_free(branch);m=(NativeSortedStringMap *)MCObjectRoot_get(&root);CHECK(m->size==1006&&MCObjectHeap_collect(h));MCObjectRoot_drop(&root);MCObjectHeap_free(h);
 h=MCObjectHeap_new(4096);CHECK(h);m=NativeSortedStringMap_new(h);CHECK(m);scope=(MCObjectRootScope){0};CHECK(MCObjectRootScope_begin(&scope,h)&&NativeSortedStringMap_beginCursor(m,&cursor));CHECK(NativeSortedStringMap_put(m,NBTString_fromASCII(h,"one"),NULL));CHECK(!NativeSortedStringMap_next(&cursor,&k,&v)&&MCObjectHeap_failed(h)&&m->size==1);MCObjectRootScope_end(&scope);MCObjectHeap_free(h);
 h=MCObjectHeap_new(4096);CHECK(h);m=NativeSortedStringMap_new(h);CHECK(m);CHECK(!NativeSortedStringMap_get(m,NULL)&&MCObjectHeap_failed(h)&&m->size==0);MCObjectHeap_free(h);
 printf("Native sorted UTF16 map: %u checks\n",checks);return 0;
}
