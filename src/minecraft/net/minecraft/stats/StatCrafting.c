#include "stats/StatCrafting.h"
#include <stdlib.h>
#include <string.h>
const MCObjectClass c919_statcrafting_class={"StatCrafting",MCObjectHeap_plainClone,StatBase_trace,NULL};
StatCrafting *StatCrafting_newIdentity(MCObjectHeap *h,const NBTString *prefix,const NBTString *suffix,const Item *item) {
    if((prefix&&((const MCObject *)prefix)->heap!=h)||(suffix&&((const MCObject *)suffix)->heap!=h)) {
        MCObjectHeap_fail(h);return NULL;
    }
    MCObjectRootScope scope={0};if(!MCObjectRootScope_begin(&scope,h))return NULL;
    if(!prefix)prefix=NBTString_literalASCII(h,"null");
    if(!suffix)suffix=NBTString_literalASCII(h,"null");
    size_t a=NBTString_length(prefix),b=NBTString_length(suffix);
    StatCrafting *s=NULL;
    if(a>SIZE_MAX-b||(a+b)>SIZE_MAX/sizeof(uint16_t)){MCObjectHeap_fail(h);goto done;}
    uint16_t *units=(a+b)?malloc((a+b)*sizeof(*units)):NULL;
    if((a+b)&&!units){MCObjectHeap_fail(h);goto done;}
    if(a)memcpy(units,NBTString_units(prefix),a*sizeof(*units));
    if(b)memcpy(units+a,NBTString_units(suffix),b*sizeof(*units));
    NBTString *id=NBTString_fromUTF16(h,units,a+b);free(units);
    if(id){s=(StatCrafting *)MCObjectHeap_alloc(h,sizeof(*s),&c919_statcrafting_class);
        if(s){s->base.statId=id;s->field_150960_a=item;}}
done:
    MCObjectRootScope_end(&scope);return MCObjectHeap_failed(h)?NULL:s;
}
const Item *StatCrafting_func_150959_a(const StatCrafting *s){return s?s->field_150960_a:NULL;}
