#include "util/NativeJavaNumber.h"
#include "util/NativeJavaString.h"
#include "nbt/NBTString.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"Native Java numbers check%u line%d %s\n",checks,__LINE__,#x);exit(1);}}while(0)
static uint64_t dbits(double value){uint64_t n;memcpy(&n,&value,8);return n;}
/* Independent Java8 API numeric goldens. UTF-16 strings preserve NUL, whitespace
   and non-ASCII digits; no platform floating/string parser is the test oracle. */
typedef struct {const uint16_t *units;size_t n;bool isDouble;uint64_t bits;bool isInt;int32_t integer;} Golden;
static const Golden golden[]={
    { (const uint16_t[]){0},0,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0074,0x0072,0x0075,0x0065},4,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0054,0x0052,0x0055,0x0045},4,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0066,0x0061,0x006c,0x0073,0x0065},5,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0031,0x002e,0x0032,0x0035},4,true,UINT64_C(0x3ff4000000000000),false,0 },
    { (const uint16_t[]){0x0020,0x0031,0x0020},3,true,UINT64_C(0x3ff0000000000000),false,0 },
    { (const uint16_t[]){0x0032,0x0031,0x0034,0x0037,0x0034,0x0038,0x0033,0x0036,0x0034,0x0038},10,true,UINT64_C(0x41e0000000000000),false,0 },
    { (const uint16_t[]){0x002d,0x0032,0x0031,0x0034,0x0037,0x0034,0x0038,0x0033,0x0036,0x0034,0x0039},11,true,UINT64_C(0xc1e0000000200000),false,0 },
    { (const uint16_t[]){0x002d,0x0030},2,true,UINT64_C(0x8000000000000000),true,0 },
    { (const uint16_t[]){0x002d,0x0030,0x002e,0x0030},4,true,UINT64_C(0x8000000000000000),false,0 },
    { (const uint16_t[]){0x004e,0x0061,0x004e},3,true,UINT64_C(0x7ff8000000000000),false,0 },
    { (const uint16_t[]){0x002d,0x004e,0x0061,0x004e},4,true,UINT64_C(0x7ff8000000000000),false,0 },
    { (const uint16_t[]){0x002b,0x004e,0x0061,0x004e},4,true,UINT64_C(0x7ff8000000000000),false,0 },
    { (const uint16_t[]){0x0049,0x006e,0x0066,0x0069,0x006e,0x0069,0x0074,0x0079},8,true,UINT64_C(0x7ff0000000000000),false,0 },
    { (const uint16_t[]){0x002d,0x0049,0x006e,0x0066,0x0069,0x006e,0x0069,0x0074,0x0079},9,true,UINT64_C(0xfff0000000000000),false,0 },
    { (const uint16_t[]){0x0030,0x0078,0x0031,0x002e,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0070,0x0031,0x0030,0x0032,0x0033},22,true,UINT64_C(0x7fefffffffffffff),false,0 },
    { (const uint16_t[]){0x0030,0x0078,0x0031,0x002e,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0066,0x0038,0x0070,0x0031,0x0030,0x0032,0x0033},23,true,UINT64_C(0x7ff0000000000000),false,0 },
    { (const uint16_t[]){0x0030,0x0078,0x0031,0x0070,0x002d,0x0031,0x0030,0x0037,0x0034},9,true,UINT64_C(0x0000000000000001),false,0 },
    { (const uint16_t[]){0x0030,0x0078,0x0031,0x0070,0x002d,0x0031,0x0030,0x0037,0x0035},9,true,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0030,0x0078,0x0031,0x002e,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0031,0x0070,0x002d,0x0031,0x0030,0x0037,0x0035},27,true,UINT64_C(0x0000000000000001),false,0 },
    { (const uint16_t[]){0x0031,0x0065,0x0033,0x0030,0x0039},5,true,UINT64_C(0x7ff0000000000000),false,0 },
    { (const uint16_t[]){0x0031,0x0065,0x002d,0x0034,0x0030,0x0030},6,true,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0031,0x002e,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0030,0x0036,0x0066},11,true,UINT64_C(0x3ff00000101b2b2a),false,0 },
    { (const uint16_t[]){0x002e,0x0031},2,true,UINT64_C(0x3fb999999999999a),false,0 },
    { (const uint16_t[]){0x0031,0x002e},2,true,UINT64_C(0x3ff0000000000000),false,0 },
    { (const uint16_t[]){0x002e},1,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x002b},1,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x004e,0x0061,0x004e,0x0064},4,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x006e,0x0061,0x006e},3,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0069,0x006e,0x0066},3,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0030,0x0078,0x0031},3,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0030,0x0078,0x0031,0x0070},4,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0031,0x0065},2,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0031,0x0065,0x002b},3,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0030,0x0078,0x002e,0x0070,0x0031},5,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0030,0x0030},2,true,UINT64_C(0x0000000000000000),true,0 },
    { (const uint16_t[]){0x0031,0x005f,0x0030},3,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0031,0x002c,0x0030},3,false,UINT64_C(0x0000000000000000),false,0 },
    { (const uint16_t[]){0x0000,0x0031,0x0000},3,true,UINT64_C(0x3ff0000000000000),false,0 },
    { (const uint16_t[]){0x0661},1,false,UINT64_C(0x0000000000000000),true,1 },
    { (const uint16_t[]){0xff11},1,false,UINT64_C(0x0000000000000000),true,1 }
};
int main(void){
    MCObjectHeap *h=MCObjectHeap_new(16u*1024u*1024u);CHECK(h);
    for(size_t i=0;i<sizeof golden/sizeof *golden;i++){
        NBTString *s=NBTString_fromUTF16(h,golden[i].units,golden[i].n);CHECK(s);
        double d=919;int32_t n=919;NativeJavaNumberResult a=NativeJavaNumber_parseDouble(s,&d),b=NativeJavaNumber_parseInt(s,&n);
        CHECK(a==(golden[i].isDouble?NATIVE_NUMBER_OK:NATIVE_NUMBER_FORMAT));CHECK(b==(golden[i].isInt?NATIVE_NUMBER_OK:NATIVE_NUMBER_FORMAT));
        CHECK(golden[i].isDouble?dbits(d)==golden[i].bits:d==919);CHECK(golden[i].isInt?n==golden[i].integer:n==919);
    }
    double d=919;int32_t n=919;bool boolean=true;
    CHECK(NativeJavaNumber_parseDouble(NULL,&d)==NATIVE_NUMBER_NULL_POINTER&&d==919);CHECK(NativeJavaNumber_parseInt(NULL,&n)==NATIVE_NUMBER_FORMAT&&n==919);CHECK(NativeJavaNumber_parseBoolean(NULL,&boolean)&&!boolean);
    const uint16_t digits[]={0x0030,0x0660,0x06f0,0x07c0,0x0966,0x09e6,0x0a66,0x0ae6,0x0b66,0x0be6,0x0c66,0x0ce6,0x0d66,0x0e50,0x0ed0,0x0f20,0x1040,0x1090,0x17e0,0x1810,0x1946,0x19d0,0x1a80,0x1a90,0x1b50,0x1bb0,0x1c40,0x1c50,0xa620,0xa8d0,0xa900,0xa9d0,0xaa50,0xabf0,0xff10};
    for(size_t i=0;i<sizeof digits/sizeof *digits;i++)for(unsigned j=0;j<10;j++){uint16_t u=(uint16_t)(digits[i]+j);NBTString *s=NBTString_fromUTF16(h,&u,1);CHECK(s&&NativeJavaNumber_parseInt(s,&n)==NATIVE_NUMBER_OK&&n==(int32_t)j);if(i)CHECK(NativeJavaNumber_parseDouble(s,&d)==NATIVE_NUMBER_FORMAT);}
    const char *truth[]={"true","TRUE","TrUe"," true","true ","1","false",""};
    for(size_t i=0;i<8;i++){NBTString *s=NBTString_fromASCII(h,truth[i]);CHECK(s&&NativeJavaNumber_parseBoolean(s,&boolean)&&boolean==(i<3));}
    char midpoint[2400]="1.00000000000000011102230246251565404236316680908203125";size_t len=strlen(midpoint);memset(midpoint+len,'0',2000);midpoint[len+2000]=0;
    CHECK(NativeJavaNumber_parseDouble(NBTString_fromASCII(h,midpoint),&d)==NATIVE_NUMBER_OK&&dbits(d)==UINT64_C(0x3ff0000000000000));midpoint[len+1999]='1';CHECK(NativeJavaNumber_parseDouble(NBTString_fromASCII(h,midpoint),&d)==NATIVE_NUMBER_OK&&dbits(d)==UINT64_C(0x3ff0000000000001));
    const uint16_t pairs[][2]={{'i',0x0131},{'i',0x0130},{'k',0x212a},{'s',0x017f},{0x03c3,0x03c2},{0xd800,0xd800},{0xd800,0xdc00}};
    for(size_t i=0;i<sizeof pairs/sizeof *pairs;i++){NBTString *a=NBTString_fromUTF16(h,&pairs[i][0],1),*b=NBTString_fromUTF16(h,&pairs[i][1],1);bool equal=false;CHECK(a&&b&&NativeJavaString_equalsIgnoreCase(a,b,&equal)&&equal==(i<6));}
    CHECK(NBTString_equalsASCII(NativeJavaString_concat(h,"prefix",NULL,"suffix"),"prefixnullsuffix"));
    for(uint32_t i=0;i<=UINT16_MAX;i++){uint16_t c=(uint16_t)i;CHECK(NativeJavaString_upper(NativeJavaString_upper(c))==NativeJavaString_upper(c));CHECK(NativeJavaString_lower(NativeJavaString_lower(c))==NativeJavaString_lower(c));}
    MCObjectHeap_free(h);printf("Native Java number/String: %u checks\n",checks);return 0;
}
