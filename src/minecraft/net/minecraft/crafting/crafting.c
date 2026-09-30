#include "crafting.h"
#include "item/item.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { int16_t id,damage; } ingredient;
typedef struct { uint8_t width,height,count; ingredient input[4]; int16_t output; uint8_t amount; int16_t damage; } recipe;
/* Numeric behavioral facts for player-grid recipes in Java 1.8.9.
   Matching and transaction code below are original implementations. */
static const recipe recipes[] = {
{ 2, 2, 4, { {-1,0} , {265,0} , {265,0} , {-1,0} }, 359, 1, 0 },
{ 2, 2, 4, { {5,32767} , {5,32767} , {5,32767} , {5,32767} }, 58, 1, 0 },
{ 2, 2, 4, { {12,0} , {12,0} , {12,0} , {12,0} }, 24, 1, 0 },
{ 2, 2, 4, { {12,1} , {12,1} , {12,1} , {12,1} }, 179, 1, 0 },
{ 2, 2, 4, { {24,0} , {24,0} , {24,0} , {24,0} }, 24, 4, 2 },
{ 2, 2, 4, { {179,0} , {179,0} , {179,0} , {179,0} }, 179, 4, 2 },
{ 2, 2, 4, { {1,0} , {1,0} , {1,0} , {1,0} }, 98, 4, 0 },
{ 2, 2, 4, { {405,0} , {405,0} , {405,0} , {405,0} }, 112, 1, 0 },
{ 2, 2, 4, { {4,32767} , {406,0} , {406,0} , {4,32767} }, 1, 2, 3 },
{ 2, 2, 4, { {3,0} , {13,32767} , {13,32767} , {3,0} }, 3, 4, 1 },
{ 2, 2, 4, { {1,3} , {1,3} , {1,3} , {1,3} }, 1, 4, 4 },
{ 2, 2, 4, { {1,1} , {1,1} , {1,1} , {1,1} }, 1, 4, 2 },
{ 2, 2, 4, { {1,5} , {1,5} , {1,5} , {1,5} }, 1, 4, 6 },
{ 2, 2, 4, { {409,0} , {409,0} , {409,0} , {409,0} }, 168, 1, 0 },
{ 2, 2, 4, { {332,0} , {332,0} , {332,0} , {332,0} }, 80, 1, 0 },
{ 2, 2, 4, { {337,0} , {337,0} , {337,0} , {337,0} }, 82, 1, 0 },
{ 2, 2, 4, { {336,0} , {336,0} , {336,0} , {336,0} }, 45, 1, 0 },
{ 2, 2, 4, { {348,0} , {348,0} , {348,0} , {348,0} }, 89, 1, 0 },
{ 2, 2, 4, { {406,0} , {406,0} , {406,0} , {406,0} }, 155, 1, 0 },
{ 2, 2, 4, { {287,0} , {287,0} , {287,0} , {287,0} }, 35, 1, 0 },
{ 2, 2, 4, { {265,0} , {265,0} , {265,0} , {265,0} }, 167, 1, 0 },
{ 2, 2, 4, { {346,0} , {-1,0} , {-1,0} , {391,0} }, 398, 1, 0 },
{ 2, 2, 4, { {415,0} , {415,0} , {415,0} , {415,0} }, 334, 1, 0 },
{ 2, 1, 2, { {54,32767} , {131,32767} , {-1,0} , {-1,0} }, 146, 1, 0 },
{ 1, 2, 2, { {44,1} , {44,1} , {-1,0} , {-1,0} }, 24, 1, 1 },
{ 1, 2, 2, { {182,0} , {182,0} , {-1,0} , {-1,0} }, 179, 1, 1 },
{ 1, 2, 2, { {44,7} , {44,7} , {-1,0} , {-1,0} }, 155, 1, 1 },
{ 1, 2, 2, { {155,0} , {155,0} , {-1,0} , {-1,0} }, 155, 2, 2 },
{ 1, 2, 2, { {44,5} , {44,5} , {-1,0} , {-1,0} }, 98, 1, 3 },
{ 2, 1, 2, { {35,0} , {35,0} , {-1,0} , {-1,0} }, 171, 3, 0 },
{ 2, 1, 2, { {35,1} , {35,1} , {-1,0} , {-1,0} }, 171, 3, 1 },
{ 2, 1, 2, { {35,2} , {35,2} , {-1,0} , {-1,0} }, 171, 3, 2 },
{ 2, 1, 2, { {35,3} , {35,3} , {-1,0} , {-1,0} }, 171, 3, 3 },
{ 2, 1, 2, { {35,4} , {35,4} , {-1,0} , {-1,0} }, 171, 3, 4 },
{ 2, 1, 2, { {35,5} , {35,5} , {-1,0} , {-1,0} }, 171, 3, 5 },
{ 2, 1, 2, { {35,6} , {35,6} , {-1,0} , {-1,0} }, 171, 3, 6 },
{ 2, 1, 2, { {35,7} , {35,7} , {-1,0} , {-1,0} }, 171, 3, 7 },
{ 2, 1, 2, { {35,8} , {35,8} , {-1,0} , {-1,0} }, 171, 3, 8 },
{ 2, 1, 2, { {35,9} , {35,9} , {-1,0} , {-1,0} }, 171, 3, 9 },
{ 2, 1, 2, { {35,10} , {35,10} , {-1,0} , {-1,0} }, 171, 3, 10 },
{ 2, 1, 2, { {35,11} , {35,11} , {-1,0} , {-1,0} }, 171, 3, 11 },
{ 2, 1, 2, { {35,12} , {35,12} , {-1,0} , {-1,0} }, 171, 3, 12 },
{ 2, 1, 2, { {35,13} , {35,13} , {-1,0} , {-1,0} }, 171, 3, 13 },
{ 2, 1, 2, { {35,14} , {35,14} , {-1,0} , {-1,0} }, 171, 3, 14 },
{ 2, 1, 2, { {35,15} , {35,15} , {-1,0} , {-1,0} }, 171, 3, 15 },
{ 0, 0, 4, { {339,0} , {339,0} , {339,0} , {334,0} }, 340, 1, 0 },
{ 0, 0, 3, { {340,0} , {351,0} , {288,0} , {-1,0} }, 386, 1, 0 },
{ 1, 2, 2, { {5,32767} , {5,32767} , {-1,0} , {-1,0} }, 280, 4, 0 },
{ 1, 2, 2, { {263,0} , {280,0} , {-1,0} , {-1,0} }, 50, 4, 0 },
{ 1, 2, 2, { {263,1} , {280,0} , {-1,0} , {-1,0} }, 50, 4, 0 },
{ 1, 2, 2, { {86,32767} , {50,32767} , {-1,0} , {-1,0} }, 91, 1, 0 },
{ 1, 2, 2, { {54,32767} , {328,0} , {-1,0} , {-1,0} }, 342, 1, 0 },
{ 1, 2, 2, { {61,32767} , {328,0} , {-1,0} , {-1,0} }, 343, 1, 0 },
{ 1, 2, 2, { {46,32767} , {328,0} , {-1,0} , {-1,0} }, 407, 1, 0 },
{ 1, 2, 2, { {154,32767} , {328,0} , {-1,0} , {-1,0} }, 408, 1, 0 },
{ 1, 2, 2, { {280,0} , {4,32767} , {-1,0} , {-1,0} }, 69, 1, 0 },
{ 1, 2, 2, { {331,0} , {280,0} , {-1,0} , {-1,0} }, 76, 1, 0 },
{ 2, 1, 2, { {1,0} , {1,0} , {-1,0} , {-1,0} }, 70, 1, 0 },
{ 2, 1, 2, { {5,32767} , {5,32767} , {-1,0} , {-1,0} }, 72, 1, 0 },
{ 2, 1, 2, { {265,0} , {265,0} , {-1,0} , {-1,0} }, 148, 1, 0 },
{ 2, 1, 2, { {266,0} , {266,0} , {-1,0} , {-1,0} }, 147, 1, 0 },
{ 1, 2, 2, { {341,0} , {33,32767} , {-1,0} , {-1,0} }, 29, 1, 0 },
{ 1, 1, 1, { {41,32767} , {-1,0} , {-1,0} , {-1,0} }, 266, 9, 0 },
{ 1, 1, 1, { {42,32767} , {-1,0} , {-1,0} , {-1,0} }, 265, 9, 0 },
{ 1, 1, 1, { {57,32767} , {-1,0} , {-1,0} , {-1,0} }, 264, 9, 0 },
{ 1, 1, 1, { {133,32767} , {-1,0} , {-1,0} , {-1,0} }, 388, 9, 0 },
{ 1, 1, 1, { {22,32767} , {-1,0} , {-1,0} , {-1,0} }, 351, 9, 4 },
{ 1, 1, 1, { {152,32767} , {-1,0} , {-1,0} , {-1,0} }, 331, 9, 0 },
{ 1, 1, 1, { {173,32767} , {-1,0} , {-1,0} , {-1,0} }, 263, 9, 0 },
{ 1, 1, 1, { {170,32767} , {-1,0} , {-1,0} , {-1,0} }, 296, 9, 0 },
{ 1, 1, 1, { {165,32767} , {-1,0} , {-1,0} , {-1,0} }, 341, 9, 0 },
{ 1, 1, 1, { {266,0} , {-1,0} , {-1,0} , {-1,0} }, 371, 9, 0 },
{ 1, 1, 1, { {360,0} , {-1,0} , {-1,0} , {-1,0} }, 362, 1, 0 },
{ 1, 1, 1, { {86,32767} , {-1,0} , {-1,0} , {-1,0} }, 361, 4, 0 },
{ 1, 1, 1, { {338,0} , {-1,0} , {-1,0} , {-1,0} }, 353, 1, 0 },
{ 1, 1, 1, { {17,0} , {-1,0} , {-1,0} , {-1,0} }, 5, 4, 0 },
{ 1, 1, 1, { {17,1} , {-1,0} , {-1,0} , {-1,0} }, 5, 4, 1 },
{ 1, 1, 1, { {17,2} , {-1,0} , {-1,0} , {-1,0} }, 5, 4, 2 },
{ 1, 1, 1, { {17,3} , {-1,0} , {-1,0} , {-1,0} }, 5, 4, 3 },
{ 1, 1, 1, { {162,0} , {-1,0} , {-1,0} , {-1,0} }, 5, 4, 4 },
{ 1, 1, 1, { {162,1} , {-1,0} , {-1,0} , {-1,0} }, 5, 4, 5 },
{ 1, 1, 1, { {1,0} , {-1,0} , {-1,0} , {-1,0} }, 77, 1, 0 },
{ 1, 1, 1, { {5,32767} , {-1,0} , {-1,0} , {-1,0} }, 143, 1, 0 },
{ 0, 0, 4, { {351,4} , {351,1} , {351,1} , {351,15} }, 351, 4, 13 },
{ 0, 0, 3, { {39,0} , {40,0} , {281,0} , {-1,0} }, 282, 1, 0 },
{ 0, 0, 3, { {86,0} , {353,0} , {344,0} , {-1,0} }, 400, 1, 0 },
{ 0, 0, 3, { {375,0} , {39,0} , {353,0} , {-1,0} }, 376, 1, 0 },
{ 0, 0, 3, { {351,0} , {351,15} , {351,15} , {-1,0} }, 351, 3, 7 },
{ 0, 0, 3, { {351,4} , {351,1} , {351,9} , {-1,0} }, 351, 3, 13 },
{ 0, 0, 3, { {289,0} , {377,0} , {263,0} , {-1,0} }, 385, 3, 0 },
{ 0, 0, 3, { {289,0} , {377,0} , {263,1} , {-1,0} }, 385, 3, 0 },
{ 0, 0, 2, { {377,0} , {341,0} , {-1,0} , {-1,0} }, 378, 1, 0 },
{ 0, 0, 2, { {98,0} , {106,0} , {-1,0} , {-1,0} }, 98, 1, 1 },
{ 0, 0, 2, { {4,0} , {106,0} , {-1,0} , {-1,0} }, 48, 1, 0 },
{ 0, 0, 2, { {1,3} , {406,0} , {-1,0} , {-1,0} }, 1, 1, 1 },
{ 0, 0, 2, { {1,3} , {4,0} , {-1,0} , {-1,0} }, 1, 2, 5 },
{ 0, 0, 2, { {351,15} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 0 },
{ 0, 0, 2, { {351,14} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 1 },
{ 0, 0, 2, { {351,13} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 2 },
{ 0, 0, 2, { {351,12} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 3 },
{ 0, 0, 2, { {351,11} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 4 },
{ 0, 0, 2, { {351,10} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 5 },
{ 0, 0, 2, { {351,9} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 6 },
{ 0, 0, 2, { {351,8} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 7 },
{ 0, 0, 2, { {351,7} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 8 },
{ 0, 0, 2, { {351,6} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 9 },
{ 0, 0, 2, { {351,5} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 10 },
{ 0, 0, 2, { {351,4} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 11 },
{ 0, 0, 2, { {351,3} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 12 },
{ 0, 0, 2, { {351,2} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 13 },
{ 0, 0, 2, { {351,1} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 14 },
{ 0, 0, 2, { {351,0} , {35,0} , {-1,0} , {-1,0} }, 35, 1, 15 },
{ 0, 0, 2, { {351,1} , {351,15} , {-1,0} , {-1,0} }, 351, 2, 9 },
{ 0, 0, 2, { {351,1} , {351,11} , {-1,0} , {-1,0} }, 351, 2, 14 },
{ 0, 0, 2, { {351,2} , {351,15} , {-1,0} , {-1,0} }, 351, 2, 10 },
{ 0, 0, 2, { {351,0} , {351,15} , {-1,0} , {-1,0} }, 351, 2, 8 },
{ 0, 0, 2, { {351,8} , {351,15} , {-1,0} , {-1,0} }, 351, 2, 7 },
{ 0, 0, 2, { {351,4} , {351,15} , {-1,0} , {-1,0} }, 351, 2, 12 },
{ 0, 0, 2, { {351,4} , {351,2} , {-1,0} , {-1,0} }, 351, 2, 6 },
{ 0, 0, 2, { {351,4} , {351,1} , {-1,0} , {-1,0} }, 351, 2, 5 },
{ 0, 0, 2, { {351,5} , {351,9} , {-1,0} , {-1,0} }, 351, 2, 13 },
{ 0, 0, 2, { {265,0} , {318,0} , {-1,0} , {-1,0} }, 259, 1, 0 },
{ 0, 0, 2, { {368,0} , {377,0} , {-1,0} , {-1,0} }, 381, 1, 0 },
{ 0, 0, 1, { {369,0} , {-1,0} , {-1,0} , {-1,0} }, 377, 2, 0 },
{ 0, 0, 1, { {37,0} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 11 },
{ 0, 0, 1, { {38,0} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 1 },
{ 0, 0, 1, { {352,0} , {-1,0} , {-1,0} , {-1,0} }, 351, 3, 15 },
{ 0, 0, 1, { {38,1} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 12 },
{ 0, 0, 1, { {38,2} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 13 },
{ 0, 0, 1, { {38,3} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 7 },
{ 0, 0, 1, { {38,4} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 1 },
{ 0, 0, 1, { {38,5} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 14 },
{ 0, 0, 1, { {38,6} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 7 },
{ 0, 0, 1, { {38,7} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 9 },
{ 0, 0, 1, { {38,8} , {-1,0} , {-1,0} , {-1,0} }, 351, 1, 7 },
{ 0, 0, 1, { {175,0} , {-1,0} , {-1,0} , {-1,0} }, 351, 2, 11 },
{ 0, 0, 1, { {175,1} , {-1,0} , {-1,0} , {-1,0} }, 351, 2, 13 },
{ 0, 0, 1, { {175,4} , {-1,0} , {-1,0} , {-1,0} }, 351, 2, 1 },
{ 0, 0, 1, { {175,5} , {-1,0} , {-1,0} , {-1,0} }, 351, 2, 9 },
};

static bool nbt_field(const mc_nbt *nbt,const char *key,mc_nbt_view *out) {
    mc_nbt_view root; return mc_nbt_root(nbt,&root) && root.type==10 && mc_nbt_find(&root,key,out);
}
static int64_t nbt_integer(const mc_nbt_view *root,const char *key) {
    mc_nbt_view field; int64_t value=0; double floating=0;
    if (root->type!=10 || !mc_nbt_find(root,key,&field)) return 0;
    if (mc_nbt_get_integer(&field,&value)) return (int32_t)(uint32_t)value;
    if (!mc_nbt_get_number(&field,&floating) || isnan(floating)) return 0;
    if (floating>=INT32_MAX) return INT32_MAX;
    if (floating<=INT32_MIN) return INT32_MIN;
    return (int32_t)floating;
}
static bool nbt_from_payload(mc_nbt *out,const mc_nbt_view *view) {
    mc_buf bytes; mc_buf_init(&bytes); mc_put_u8(&bytes,view->type); mc_put_i16(&bytes,0);
    mc_put_bytes(&bytes,view->data,view->size);
    bool ok=!bytes.failed && mc_nbt_read(&bytes,out); mc_buf_free(&bytes); return ok;
}
/* Replace one named child, preserving the original root name and every other
   encoded child verbatim, including fields unknown to this implementation. */
static bool nbt_replace(mc_nbt *out,const char *key,const mc_nbt_view *value) {
    mc_buf bytes; mc_buf_init(&bytes); mc_nbt_view root;
    if (out->size) {
        if (!mc_nbt_root(out,&root) || root.type!=10) { mc_buf_free(&bytes); return false; }
        mc_put_bytes(&bytes,out->data,(size_t)(root.data-out->data));
        mc_buf input={(uint8_t *)root.data,root.size,root.size,0,false};
        while (input.pos<input.len) {
            mc_nbt child; mc_nbt_init(&child);
            if (!mc_nbt_read(&input,&child)) { mc_nbt_free(&child); bytes.failed=true; break; }
            if (!child.size) { mc_nbt_free(&child); break; }
            size_t length=((size_t)child.data[1]<<8)|child.data[2];
            if (length!=strlen(key) || memcmp(child.data+3,key,length)) mc_put_bytes(&bytes,child.data,child.size);
            mc_nbt_free(&child);
        }
    } else { mc_put_u8(&bytes,10); mc_put_i16(&bytes,0); }
    if (value) {
        mc_put_u8(&bytes,value->type); mc_put_i16(&bytes,(int16_t)strlen(key));
        mc_put_bytes(&bytes,key,strlen(key)); mc_put_bytes(&bytes,value->data,value->size);
    }
    mc_put_u8(&bytes,0); bool ok=!bytes.failed && mc_nbt_read(&bytes,out); mc_buf_free(&bytes); return ok;
}
static bool nbt_number(mc_nbt *out,const char *key,uint8_t type,int32_t value) {
    mc_buf bytes; mc_buf_init(&bytes);
    if (type==1) mc_put_u8(&bytes,(uint8_t)value); else mc_put_i32(&bytes,value);
    mc_nbt_view view={type,bytes.data,bytes.len}; bool ok=!bytes.failed && nbt_replace(out,key,&view);
    mc_buf_free(&bytes); return ok;
}
static bool nbt_string(mc_nbt *out,const char *key,const char *value) {
    mc_buf bytes; mc_buf_init(&bytes); mc_put_i16(&bytes,(int16_t)strlen(value)); mc_put_bytes(&bytes,value,strlen(value));
    mc_nbt_view view={8,bytes.data,bytes.len}; bool ok=!bytes.failed && nbt_replace(out,key,&view);
    mc_buf_free(&bytes); return ok;
}
static bool nbt_nested(mc_nbt *out,const char *key,const mc_nbt *child) {
    mc_nbt_view view; return mc_nbt_root(child,&view) && nbt_replace(out,key,&view);
}
static bool nbt_array(mc_nbt *out,const char *key,const int32_t *colors,unsigned count) {
    mc_buf bytes; mc_buf_init(&bytes); mc_put_i32(&bytes,(int32_t)count);
    for (unsigned i=0;i<count;i++) mc_put_i32(&bytes,colors[i]);
    mc_nbt_view view={11,bytes.data,bytes.len}; bool ok=!bytes.failed && nbt_replace(out,key,&view);
    mc_buf_free(&bytes); return ok;
}
static bool nbt_append(mc_nbt *out,const char *key,const mc_nbt_view *existing,const mc_nbt *child) {
    mc_nbt_view item; if (!mc_nbt_root(child,&item) || item.type!=10) return false;
    mc_buf bytes; mc_buf_init(&bytes); unsigned count=0;
    if (existing && existing->type==9 && existing->size>=5 && existing->data[0]==10) {
        count=((unsigned)existing->data[1]<<24)|((unsigned)existing->data[2]<<16)|
              ((unsigned)existing->data[3]<<8)|existing->data[4];
    }
    mc_put_u8(&bytes,10); mc_put_i32(&bytes,(int32_t)(count+1));
    if (count) mc_put_bytes(&bytes,existing->data+5,existing->size-5);
    mc_put_bytes(&bytes,item.data,item.size); mc_nbt_view view={9,bytes.data,bytes.len};
    bool ok=!bytes.failed && nbt_replace(out,key,&view); mc_buf_free(&bytes); return ok;
}
static unsigned list_count(const mc_nbt_view *list) {
    return list->type==9 && list->size>=5 ? ((unsigned)list->data[1]<<24)|((unsigned)list->data[2]<<16)|
           ((unsigned)list->data[3]<<8)|list->data[4] : 0;
}
static unsigned durability(int id) {
    if (id>=268 && id<=271) return 59;
    if (id>=272 && id<=275) return 131;
    if (id>=276 && id<=279) return 1561;
    if (id>=283 && id<=286) return 32;
    if (id>=298 && id<=317) {
        static const unsigned armor[]={55,80,75,65,165,240,225,195,165,240,225,195,363,528,495,429,77,112,105,91};
        return armor[id-298];
    }
    switch (id) {
        case 256: case 257: case 258: case 267: case 292: return 250;
        case 259: case 346: return 64; case 261: return 384;
        case 290: return 59; case 291: return 131; case 293: return 1561;
        case 294: return 32; case 359: return 238; case 398: return 25;
        default: return 0;
    }
}
static bool ingredient_matches(const ingredient *expected,const mc_slot *actual) {
    return expected->id==actual->item_id && (expected->id==-1 || expected->damage==32767 || expected->damage==actual->damage);
}
static bool recipe_matches(const recipe *rule,const mc_slot *grid,unsigned width,unsigned height) {
    if (rule->width) {
        if (rule->width>width || rule->height>height) return false;
        for (unsigned oy=0;oy<=height-rule->height;oy++) for (unsigned ox=0;ox<=width-rule->width;ox++) {
            for (unsigned mirror=0;mirror<2;mirror++) {
                bool matched=true;
                for (unsigned y=0;y<height;y++) for (unsigned x=0;x<width;x++) {
                    ingredient empty={-1,0}; const ingredient *expected=&empty;
                    if (x>=ox && x<ox+rule->width && y>=oy && y<oy+rule->height) {
                        unsigned rx=x-ox; if (mirror) rx=rule->width-1-rx;
                        expected=&rule->input[(y-oy)*rule->width+rx];
                    }
                    if (!ingredient_matches(expected,&grid[y*width+x])) matched=false;
                }
                if (matched) return true;
            }
        }
        return false;
    }
    unsigned used=0,occupied=0;
    for (unsigned i=0;i<width*height;i++) {
        if (grid[i].item_id==-1) continue;
        occupied++; bool matched=false;
        for (unsigned j=0;j<rule->count;j++) if (!(used&(1u<<j)) && ingredient_matches(&rule->input[j],&grid[i])) {
            used|=1u<<j; matched=true; break;
        }
        if (!matched) return false;
    }
    return occupied==rule->count;
}

static const int32_t firework_colors[]={1973019,11743532,3887386,5320730,2437522,8073150,2651799,11250603,
    4408131,14188952,4312372,14602026,6719955,12801229,15435844,15790320};
static const int32_t leather_colors[]={1644825,10040115,6717235,6704179,3361970,8339378,5013401,10066329,
    5000268,15892389,8375321,15066419,6724056,11685080,14188339,16777215};
static unsigned dye_index(int damage) { return damage>=0 && damage<16 ? (unsigned)damage : 0; }

/* -1: allocation/encoding failure, 0: unmatched, 1: matched. */
static int leather_recipe(const mc_slot *grid,unsigned count,mc_slot *result) {
    const mc_slot *armor=NULL; int channels[3]={0,0,0},brightness=0,samples=0,dyes=0;
    for (unsigned i=0;i<count;i++) {
        const mc_slot *item=&grid[i]; if (item->item_id==-1) continue;
        if (item->item_id>=298 && item->item_id<=301) { if (armor) return 0; armor=item; }
        else if (item->item_id==351) dyes++; else return 0;
    }
    if (!armor || !dyes) return 0;
    mc_nbt display; mc_nbt_init(&display); mc_nbt_view view,color;
    if (nbt_field(&armor->nbt,"display",&view) && view.type==10) {
        if (!nbt_from_payload(&display,&view)) return -1;
        if (mc_nbt_find(&view,"color",&color) && color.type>=1 && color.type<=6) {
            int64_t old=nbt_integer(&view,"color");
            int r=((int)old>>16)&255,g=((int)old>>8)&255,b=(int)old&255;
            channels[0]+=r; channels[1]+=g; channels[2]+=b; brightness+=r>g ? (r>b?r:b) : (g>b?g:b); samples++;
        }
    }
    for (unsigned i=0;i<count;i++) if (grid[i].item_id==351) {
        int value=leather_colors[dye_index(grid[i].damage)],r=(value>>16)&255,g=(value>>8)&255,b=value&255;
        channels[0]+=r; channels[1]+=g; channels[2]+=b; brightness+=r>g ? (r>b?r:b) : (g>b?g:b); samples++;
    }
    for (unsigned i=0;i<3;i++) channels[i]/=samples;
    float average=(float)brightness/(float)samples;
    int maximum=channels[0]>channels[1] ? channels[0] : channels[1]; if (channels[2]>maximum) maximum=channels[2];
    if (maximum) for (unsigned i=0;i<3;i++) channels[i]=(int)((float)channels[i]*average/(float)maximum);
    int mixed=(channels[0]<<16)|(channels[1]<<8)|channels[2];
    bool ok=mc_slot_copy(result,armor); if (ok) result->count=1;
    if (ok) ok=nbt_number(&display,"color",3,mixed) && nbt_nested(&result->nbt,"display",&display);
    mc_nbt_free(&display); return ok ? 1 : -1;
}
static int fireworks_recipe(const mc_slot *grid,unsigned count,mc_slot *result) {
    unsigned powder=0,paper=0,stars=0,dyes=0,shapes=0,trail=0,flicker=0; int type=0; const mc_slot *star=NULL;
    int32_t colors[9];
    for (unsigned i=0;i<count;i++) {
        const mc_slot *item=&grid[i];
        switch (item->item_id) {
            case -1: break; case 289: powder++; break; case 339: paper++; break;
            case 402: stars++; star=item; break;
            case 351: colors[dyes++]=firework_colors[dye_index(item->damage)]; break;
            case 264: trail++; break; case 348: flicker++; break;
            case 385: shapes++; type=1; break; case 371: shapes++; type=2; break;
            case 397: shapes++; type=3; break; case 288: shapes++; type=4; break;
            default: return 0;
        }
    }
    mc_nbt inner; mc_nbt_init(&inner); bool ok=true; int matched=0;
    if (powder>=1 && powder<=3 && paper==1 && !dyes && !shapes && !trail && !flicker) {
        matched=1; ok=mc_slot_set(result,401,1,0);
        if (ok && stars) {
            const uint8_t empty[]={0,0,0,0,0}; mc_nbt_view list={9,empty,sizeof(empty)};
            ok=nbt_number(&inner,"Flight",1,(int32_t)powder) && nbt_replace(&inner,"Explosions",&list);
        }
        for (unsigned i=0;i<count && ok;i++) if (grid[i].item_id==402) {
            mc_nbt_view explosion,list;
            if (nbt_field(&grid[i].nbt,"Explosion",&explosion) && explosion.type==10) {
                mc_nbt owned; mc_nbt_init(&owned); ok=nbt_from_payload(&owned,&explosion);
                bool existing=nbt_field(&inner,"Explosions",&list);
                if (ok) ok=nbt_append(&inner,"Explosions",existing ? &list : NULL,&owned);
                mc_nbt_free(&owned);
            }
        }
        if (ok && stars) ok=nbt_nested(&result->nbt,"Fireworks",&inner);
    } else if (powder==1 && !paper && !stars && dyes && shapes<=1 && trail<=1 && flicker<=1) {
        matched=1; ok=mc_slot_set(result,402,1,0) && nbt_number(&inner,"Type",1,type) && nbt_array(&inner,"Colors",colors,dyes);
        if (ok && trail) ok=nbt_number(&inner,"Trail",1,1);
        if (ok && flicker) ok=nbt_number(&inner,"Flicker",1,1);
        if (ok) ok=nbt_nested(&result->nbt,"Explosion",&inner);
    } else if (!powder && !paper && stars==1 && dyes && !shapes && !trail && !flicker) {
        mc_nbt_view explosion;
        if (star->nbt.size) {
            matched=1; ok=mc_slot_copy(result,star); if (ok) result->count=1;
            if (ok && nbt_field(&star->nbt,"Explosion",&explosion) && explosion.type==10)
                ok=nbt_from_payload(&inner,&explosion) && nbt_array(&inner,"FadeColors",colors,dyes) && nbt_nested(&result->nbt,"Explosion",&inner);
        }
    }
    mc_nbt_free(&inner); return !ok ? -1 : matched;
}
static unsigned banner_patterns(const mc_slot *banner,mc_nbt_view *bet,mc_nbt_view *patterns) {
    if (!nbt_field(&banner->nbt,"BlockEntityTag",bet) || bet->type!=10 ||
        !mc_nbt_find(bet,"Patterns",patterns) || patterns->type!=9 || patterns->data[0]!=10) return 0;
    return list_count(patterns);
}
static int banner_color(const mc_slot *banner) {
    mc_nbt_view bet,base;
    if (nbt_field(&banner->nbt,"BlockEntityTag",&bet) && bet.type==10 &&
        mc_nbt_find(&bet,"Base",&base) && base.type>=1 && base.type<=6) return (int)nbt_integer(&bet,"Base");
    return banner->damage&15;
}
static int banner_recipe(const mc_slot *grid,unsigned count,mc_slot *result,mc_slot *remaining) {
    int banner=-1,blank=-1,special=-1,dye=-1; unsigned occupied=0;
    mc_nbt_view bet,patterns; unsigned pattern_count=0;
    for (unsigned i=0;i<count;i++) {
        if (grid[i].item_id==-1) continue;
        occupied++;
        if (grid[i].item_id==425) {
            unsigned n=banner_patterns(&grid[i],&bet,&patterns);
            if (n) { if (banner>=0) return 0; banner=(int)i; pattern_count=n; }
            else { if (blank>=0) return 0; blank=(int)i; }
        } else if (grid[i].item_id==351) { if (dye>=0) return 0; dye=(int)i; }
        else { if (special>=0) return 0; special=(int)i; }
    }
    if (occupied==2 && banner>=0 && blank>=0 && banner_color(&grid[banner])==banner_color(&grid[blank])) {
        bool ok=mc_slot_copy(result,&grid[banner]) && mc_slot_copy(&remaining[banner],&grid[banner]);
        if (ok) result->count=remaining[banner].count=1;
        return ok ? 1 : -1;
    }
    if (occupied!=3 || (banner>=0 && blank>=0) || (banner<0 && blank<0) || dye<0 || special<0 || pattern_count>=6) return 0;
    const mc_slot *ingredient_item=&grid[special]; const char *pattern=NULL;
    if (ingredient_item->item_id==45 && ingredient_item->damage==0) pattern="bri";
    if (ingredient_item->item_id==106 && ingredient_item->damage==0) pattern="cbo";
    if (ingredient_item->item_id==397 && ingredient_item->damage==1) pattern="sku";
    if (ingredient_item->item_id==397 && ingredient_item->damage==4) pattern="cre";
    if (ingredient_item->item_id==38 && ingredient_item->damage==8) pattern="flo";
    if (ingredient_item->item_id==322 && ingredient_item->damage==1) pattern="moj";
    if (!pattern) return 0;
    int source=banner>=0 ? banner : blank; mc_nbt inner,entry; mc_nbt_init(&inner); mc_nbt_init(&entry);
    bool ok=mc_slot_copy(result,&grid[source]); if (ok) result->count=1;
    bool existing=nbt_field(&grid[source].nbt,"BlockEntityTag",&bet) && bet.type==10;
    if (ok && existing) ok=nbt_from_payload(&inner,&bet);
    bool old=nbt_field(&inner,"Patterns",&patterns);
    if (ok) ok=nbt_string(&entry,"Pattern",pattern) && nbt_number(&entry,"Color",3,grid[dye].damage) &&
               nbt_append(&inner,"Patterns",old ? &patterns : NULL,&entry) && nbt_nested(&result->nbt,"BlockEntityTag",&inner);
    mc_nbt_free(&inner); mc_nbt_free(&entry); return ok ? 1 : -1;
}
static int cloning_recipe(const mc_slot *grid,unsigned count,mc_slot *result,mc_slot *remaining,bool book) {
    int source=-1; unsigned blanks=0;
    for (unsigned i=0;i<count;i++) {
        int id=grid[i].item_id; if (id==-1) continue;
        if (id==(book ? 387 : 358)) { if (source>=0) return 0; source=(int)i; }
        else if (id==(book ? 386 : 395)) blanks++; else return 0;
    }
    if (source<0 || !blanks) return 0;
    mc_nbt_view root; int64_t generation=0;
    if (book) {
        if (!mc_nbt_root(&grid[source].nbt,&root) || root.type!=10) return 0;
        generation=nbt_integer(&root,"generation"); if (generation>=2) return 0;
    }
    bool ok=mc_slot_copy(result,&grid[source]);
    if (ok) result->count=(uint8_t)(blanks+(book ? 0u : 1u));
    if (ok && book) {
        ok=nbt_number(&result->nbt,"generation",3,(int32_t)(generation+1)) && mc_slot_copy(&remaining[source],&grid[source]);
        if (ok) remaining[source].count=1;
    }
    return ok ? 1 : -1;
}
static int repair_recipe(const mc_slot *grid,unsigned count,mc_slot *result) {
    const mc_slot *first=NULL,*second=NULL;
    for (unsigned i=0;i<count;i++) if (grid[i].item_id!=-1) {
        if (!first) first=&grid[i]; else if (!second) second=&grid[i]; else return 0;
    }
    if (!first || !second || first->item_id!=second->item_id || first->count!=1 || second->count!=1) return 0;
    unsigned max=durability(first->item_id); if (!max) return 0;
    int damage=(int)first->damage+(int)second->damage-(int)max-(int)(max*5u/100u); if (damage<0) damage=0;
    if (damage>INT16_MAX) return 0;
    return mc_slot_set(result,first->item_id,1,(int16_t)damage) ? 1 : -1;
}

bool mc_crafting_match(const mc_slot *grid,unsigned width,unsigned height,mc_slot *result,mc_slot *remaining) {
    if (!grid || !result || !remaining || !width || !height || width>3 || height>3) return false;
    unsigned count=width*height; mc_slot output,left[9],validate; mc_slot_init(&output); mc_slot_init(&validate);
    for (unsigned i=0;i<count;i++) mc_slot_init(&left[i]);
    bool ok=true;
    for (unsigned i=0;i<count;i++) {
        if (result==&grid[i] || result==&remaining[i] || !mc_slot_copy(&validate,&grid[i])) ok=false;
        for (unsigned j=0;j<count;j++) if (&remaining[i]==&grid[j]) ok=false;
    }
    mc_slot_free(&validate); int matched=0;
    if (ok) matched=leather_recipe(grid,count,&output);
    if (ok && !matched) matched=fireworks_recipe(grid,count,&output);
    if (ok && !matched) matched=banner_recipe(grid,count,&output,left);
    if (ok && !matched) matched=cloning_recipe(grid,count,&output,left,true);
    if (ok && !matched) matched=cloning_recipe(grid,count,&output,left,false);
    if (ok && !matched) for (size_t i=0;i<sizeof(recipes)/sizeof(recipes[0]);i++) {
        if (recipe_matches(&recipes[i],grid,width,height)) {
            matched=mc_slot_set(&output,recipes[i].output,recipes[i].amount,recipes[i].damage) ? 1 : -1; break;
        }
    }
    if (ok && !matched) matched=repair_recipe(grid,count,&output);
    if (matched<0) ok=false;
    if (ok && matched) for (unsigned i=0;i<count;i++) if (left[i].item_id==-1 &&
        (grid[i].item_id==326 || grid[i].item_id==327 || grid[i].item_id==335)) ok=mc_slot_set(&left[i],325,1,0);
    if (ok) {
        mc_slot_free(result); *result=output; mc_slot_init(&output);
        for (unsigned i=0;i<count;i++) { mc_slot_free(&remaining[i]); remaining[i]=left[i]; mc_slot_init(&left[i]); }
    }
    mc_slot_free(&output); for (unsigned i=0;i<count;i++) mc_slot_free(&left[i]); return ok;
}
bool mc_crafting_update(mc_inventory *inventory) {
    if (!inventory) return false;
    mc_slot output,left[4]; mc_slot_init(&output); for (unsigned i=0;i<4;i++) mc_slot_init(&left[i]);
    bool ok=mc_crafting_match(&inventory->slots[1],2,2,&output,left);
    if (ok) { mc_slot_free(&inventory->slots[0]); inventory->slots[0]=output; mc_slot_init(&output); }
    mc_slot_free(&output); for (unsigned i=0;i<4;i++) mc_slot_free(&left[i]); return ok;
}

void mc_crafting_effects_init(mc_crafting_effects *effects) {
    effects->count=0; for (unsigned i=0;i<MC_CRAFTING_MAX_EFFECTS;i++) mc_slot_init(&effects->dropped[i]);
}
void mc_crafting_effects_free(mc_crafting_effects *effects) {
    for (unsigned i=0;i<MC_CRAFTING_MAX_EFFECTS;i++) mc_slot_free(&effects->dropped[i]);
    effects->count=0;
}
static bool append_drop(mc_crafting_effects *effects,const mc_slot *item) {
    if (item->item_id==-1) return true;
    if (effects->count>=MC_CRAFTING_MAX_EFFECTS || !mc_slot_copy(&effects->dropped[effects->count],item)) return false;
    effects->count++; return true;
}
static void consume_slot(mc_slot *slot,unsigned count) {
    slot->count=(uint8_t)(slot->count-count); if (!slot->count) mc_slot_free(slot);
}
static bool craft_can_stack(const mc_slot *a,const mc_slot *b) {
    return a->item_id>=0 && a->item_id==b->item_id &&
           (!mc_item_has_subtypes(a->item_id) || a->damage==b->damage) && mc_nbt_equal(&a->nbt,&b->nbt);
}
static bool transfer(mc_slot *source,mc_slot *destination,unsigned count) {
    if (!count) return true;
    if (count>source->count || (destination->item_id!=-1 &&
        (!craft_can_stack(source,destination) || (unsigned)destination->count+count>127))) return false;
    if (destination->item_id==-1) { if (!mc_slot_copy(destination,source)) return false; destination->count=(uint8_t)count; }
    else destination->count=(uint8_t)(destination->count+count);
    consume_slot(source,count); return true;
}
/* Output shift merge visits the player inventory backwards, as ContainerPlayer
   does. Existing stacks obey their item cap; an empty slot accepts the output. */
static bool shift_insert(mc_inventory *inventory,mc_slot *source) {
    for (unsigned pass=0;pass<2 && source->item_id!=-1;pass++) for (int i=44;i>=9 && source->item_id!=-1;i--) {
        mc_slot *target=&inventory->slots[i]; bool empty=target->item_id==-1;
        if ((pass==0 && empty) || (pass==1 && !empty) || (!empty && !craft_can_stack(source,target))) continue;
        unsigned limit=mc_item_stack_limit(source->item_id),amount=source->count;
        if (!empty) { unsigned room=target->count<limit ? limit-target->count : 0; if (amount>room) amount=room; }
        if (!transfer(source,target,amount)) return false;
    }
    return true;
}
static bool insert_player(mc_inventory *inventory,mc_slot *source) {
    for (unsigned pass=0;pass<2 && source->item_id!=-1;pass++) for (unsigned j=0;j<36 && source->item_id!=-1;j++) {
        unsigned i=j<9 ? 36+j : j; mc_slot *target=&inventory->slots[i]; bool empty=target->item_id==-1;
        if ((pass==0 && empty) || (pass==1 && !empty) || (!empty && !craft_can_stack(source,target))) continue;
        unsigned limit=mc_item_stack_limit(source->item_id),room=empty ? limit : (target->count<limit ? limit-target->count : 0);
        unsigned amount=source->count<room ? source->count : room;
        if (!transfer(source,target,amount)) return false;
    }
    return true;
}
static bool consume_recipe(mc_inventory *inventory,mc_crafting_effects *effects) {
    mc_slot result,left[4]; mc_slot_init(&result); for (unsigned i=0;i<4;i++) mc_slot_init(&left[i]);
    bool ok=mc_crafting_match(&inventory->slots[1],2,2,&result,left) && result.item_id!=-1;
    if (ok) for (unsigned i=0;i<4 && ok;i++) {
        mc_slot *input=&inventory->slots[i+1]; if (input->item_id!=-1) consume_slot(input,1);
        if (left[i].item_id==-1) continue;
        if (input->item_id==-1) { *input=left[i]; mc_slot_init(&left[i]); }
        else if (mc_slot_can_stack(input,&left[i]) && (unsigned)input->count+left[i].count<=127) {
            input->count=(uint8_t)(input->count+left[i].count); mc_slot_free(&left[i]);
        } else ok=insert_player(inventory,&left[i]) && append_drop(effects,&left[i]);
    }
    if (ok) ok=mc_crafting_update(inventory);
    mc_slot_free(&result); for (unsigned i=0;i<4;i++) mc_slot_free(&left[i]); return ok;
}
static bool output_action(mc_inventory *inventory,int button,int mode,mc_slot *returned,mc_crafting_effects *effects) {
    mc_slot *output=&inventory->slots[0],*cursor=&inventory->cursor;
    if (mode==5) {
        mc_slot dropped; mc_slot_init(&dropped);
        bool ok=mc_inventory_click_result(inventory,0,button,mode,returned,&dropped) && append_drop(effects,&dropped);
        mc_slot_free(&dropped); return ok;
    }
    if (inventory->drag_active && mode!=5) {
        inventory->drag_active=false; inventory->drag_mode=0; inventory->drag_slots=0; return true;
    }
    if (output->item_id==-1) {
        if (mode==6) {
            mc_slot dropped; mc_slot_init(&dropped);
            bool ok=mc_inventory_click_result(inventory,0,button,mode,returned,&dropped) && append_drop(effects,&dropped);
            mc_slot_free(&dropped); return ok && mc_crafting_update(inventory);
        }
        return true;
    }
    if (mode==0) {
        if (!mc_slot_copy(returned,output)) return false;
        if (cursor->item_id!=-1 && (!craft_can_stack(cursor,output) ||
            (unsigned)cursor->count+output->count>mc_item_stack_limit(output->item_id))) return true;
        mc_slot crafted; mc_slot_init(&crafted); bool ok=mc_slot_copy(&crafted,output);
        if (ok) ok=transfer(&crafted,cursor,crafted.count) && consume_recipe(inventory,effects);
        mc_slot_free(&crafted); return ok;
    }
    if (mode==1) {
        int16_t id=output->item_id,damage=output->damage; bool first=true;
        while (output->item_id==id && output->damage==damage) {
            mc_slot crafted; mc_slot_init(&crafted); bool ok=mc_slot_copy(&crafted,output);
            unsigned before=crafted.count;
            if (ok) ok=shift_insert(inventory,&crafted);
            if (!ok) { mc_slot_free(&crafted); return false; }
            if (crafted.count==before) { mc_slot_free(&crafted); return true; }
            if (first) { if (!mc_slot_copy(returned,output)) { mc_slot_free(&crafted); return false; } first=false; }
            /* Legacy 1.8.9 consumes one whole recipe after a partial insertion;
               uninserted result items are discarded rather than dropped. */
            mc_slot_free(&crafted); if (!consume_recipe(inventory,effects)) return false;
        }
        return true;
    }
    if (mode==2) {
        mc_slot *hotbar=&inventory->slots[36+button];
        if (hotbar->item_id!=-1) {
            bool empty=false; for (unsigned i=9;i<45;i++) if (inventory->slots[i].item_id==-1) empty=true;
            if (!empty) return true;
        }
        mc_slot displaced; mc_slot_init(&displaced); bool ok=mc_slot_copy(&displaced,hotbar) && mc_slot_copy(hotbar,output);
        if (ok) ok=consume_recipe(inventory,effects) && insert_player(inventory,&displaced) && append_drop(effects,&displaced);
        mc_slot_free(&displaced); return ok;
    }
    if (mode==3) {
        if (cursor->item_id==-1) {
            if (!mc_slot_copy(cursor,output)) return false;
            cursor->count=(uint8_t)mc_item_stack_limit(cursor->item_id);
        }
        return true;
    }
    if (mode==4) return cursor->item_id!=-1 || (append_drop(effects,output) && consume_recipe(inventory,effects));
    if (mode==5 || mode==6) return true;
    return false;
}
static bool output_disjoint(const mc_inventory *inventory,const mc_slot *returned,const mc_crafting_effects *effects) {
    if (returned==&inventory->cursor) return false;
    for (unsigned i=0;i<45;i++) if (returned==&inventory->slots[i]) return false;
    for (unsigned j=0;j<MC_CRAFTING_MAX_EFFECTS;j++) {
        if (returned==&effects->dropped[j] || &effects->dropped[j]==&inventory->cursor) return false;
        for (unsigned i=0;i<45;i++) if (&effects->dropped[j]==&inventory->slots[i]) return false;
    }
    return true;
}
bool mc_crafting_click(mc_inventory *inventory,int index,int button,int mode,mc_slot *returned,mc_crafting_effects *effects) {
    if (!inventory || !returned || !effects || !output_disjoint(inventory,returned,effects)) return false;
    if (index==0 && !(((mode==0 || mode==1 || mode==4 || mode==6) && (button==0 || button==1)) ||
        (mode==2 && button>=0 && button<9) || (mode==3 && button==2) ||
        (mode==5 && button>=0 && button<=10 && (button&3)==1))) return false;
    mc_inventory working; mc_inventory_init(&working); mc_slot answer,drop; mc_slot_init(&answer); mc_slot_init(&drop);
    mc_crafting_effects pending; mc_crafting_effects_init(&pending);
    bool ok=mc_inventory_copy(&working,inventory) && mc_crafting_update(&working);
    if (ok && index==0) ok=output_action(&working,button,mode,&answer,&pending);
    else if (ok) ok=mc_inventory_click_result(&working,index,button,mode,&answer,&drop) && append_drop(&pending,&drop) && mc_crafting_update(&working);
    if (ok) {
        mc_inventory_free(inventory); *inventory=working; mc_inventory_init(&working);
        mc_slot_free(returned); *returned=answer; mc_slot_init(&answer);
        mc_crafting_effects_free(effects); *effects=pending; mc_crafting_effects_init(&pending);
    }
    mc_inventory_free(&working); mc_slot_free(&answer); mc_slot_free(&drop); mc_crafting_effects_free(&pending); return ok;
}
bool mc_crafting_close(mc_inventory *inventory,mc_crafting_effects *effects) {
    if (!inventory || !effects) return false;
    mc_slot dummy; mc_slot_init(&dummy); if (!output_disjoint(inventory,&dummy,effects)) return false;
    mc_inventory working; mc_inventory_init(&working); mc_crafting_effects pending; mc_crafting_effects_init(&pending);
    bool ok=mc_inventory_copy(&working,inventory) && append_drop(&pending,&working.cursor);
    for (unsigned i=1;i<=4 && ok;i++) ok=append_drop(&pending,&working.slots[i]);
    if (ok) {
        for (unsigned i=0;i<=4;i++) mc_slot_free(&working.slots[i]);
        mc_slot_free(&working.cursor);
        working.drag_active=false; working.drag_mode=0; working.drag_slots=0;
        mc_inventory_free(inventory); *inventory=working; mc_inventory_init(&working);
        mc_crafting_effects_free(effects); *effects=pending; mc_crafting_effects_init(&pending);
    }
    mc_inventory_free(&working); mc_crafting_effects_free(&pending); return ok;
}
