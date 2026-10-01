#include "util/NativeJavaNumber.h"
#include "nbt/NBTInternal.h"
#include <limits.h>
#include <string.h>

static bool valid(const NBTString *s) {
    if(!s)return true;
    size_t n=MCObjectHeap_objectSize((const MCObject *)s);
    bool ok=NBTString_isInstance((const MCObject *)s)&&n>=sizeof *s&&
        s->length<=(n-sizeof *s)/sizeof *s->units&&!MCObjectHeap_failed(s->object.heap);
    if(!ok)MCObjectHeap_fail(s->object.heap);
    return ok;
}
bool NativeJavaNumber_parseBoolean(const NBTString *s,bool *out) {
    if(!out||!valid(s))return false;
    bool value=s&&s->length==4;
    static const uint16_t letters[4]={'t','r','u','e'};
    for(size_t i=0;value&&i<4;i++) {
        uint16_t c=s->units[i];
        value=c==letters[i]||c==(uint16_t)(letters[i]-32);
    }
    *out=value;return true;
}
static int decimalDigit(uint16_t c) {
    /* Java8 Character.digit(char,10) numeric facts, independently checked for
       every BMP code unit. Supplementary pairs are not combined by parseInt. */
    static const uint16_t starts[]={0x0030,0x0660,0x06f0,0x07c0,0x0966,0x09e6,
        0x0a66,0x0ae6,0x0b66,0x0be6,0x0c66,0x0ce6,0x0d66,0x0e50,0x0ed0,
        0x0f20,0x1040,0x1090,0x17e0,0x1810,0x1946,0x19d0,0x1a80,0x1a90,
        0x1b50,0x1bb0,0x1c40,0x1c50,0xa620,0xa8d0,0xa900,0xa9d0,0xaa50,
        0xabf0,0xff10};
    for(size_t i=0;i<sizeof starts/sizeof *starts;i++)
        if(c>=starts[i]&&c-starts[i]<10)return (int)(c-starts[i]);
    return -1;
}
NativeJavaNumberResult NativeJavaNumber_parseInt(const NBTString *s,int32_t *out) {
    if(!out||!valid(s))return NATIVE_NUMBER_FAILURE;
    if(!s||!s->length)return NATIVE_NUMBER_FORMAT;
    size_t i=0;bool negative=false;
    if(s->units[0]=='-'||s->units[0]=='+') {negative=s->units[0]=='-';i++;}
    if(i==s->length)return NATIVE_NUMBER_FORMAT;
    uint32_t number=0,limit=negative?UINT32_C(2147483648):INT32_MAX;
    for(;i<s->length;i++) {
        int digit=decimalDigit(s->units[i]);
        if(digit<0||number>(limit-(uint32_t)digit)/10)return NATIVE_NUMBER_FORMAT;
        number=number*10+(uint32_t)digit;
    }
    int32_t result=negative?(number==UINT32_C(2147483648)?INT32_MIN:-(int32_t)number):(int32_t)number;
    *out=result;return NATIVE_NUMBER_OK;
}

/* Independent exact-rational IEEE binary64 parser. Decimal midpoints of the
   binary64 lattice terminate in at most 1075 decimal places; 1100 significant
   digits preserve every midpoint plus a sticky tail. Hex retains 280 digits.
   Fixed scratch integers are native transient workspace, not gameplay owners.
   Grammar/rounding do not depend on host strtod, locale or floating arithmetic. */
#define BIG_WORDS 192u
#define DECIMAL_KEEP 1100u
#define HEX_KEEP 280u
typedef struct {uint32_t words[BIG_WORDS];size_t count;} Big;
static void small(Big *a,uint32_t value) {memset(a,0,sizeof *a);a->words[0]=value;a->count=value?1:0;}
static bool multiply(Big *a,uint32_t factor,uint32_t add) {
    uint64_t carry=add;
    for(size_t i=0;i<a->count;i++) {
        uint64_t n=(uint64_t)a->words[i]*factor+carry;a->words[i]=(uint32_t)n;carry=n>>32;
    }
    if(carry) {if(a->count==BIG_WORDS)return false;a->words[a->count++]=(uint32_t)carry;}
    return true;
}
static int compare(const Big *a,const Big *b) {
    if(a->count!=b->count)return a->count>b->count?1:-1;
    for(size_t i=a->count;i>0;i--)if(a->words[i-1]!=b->words[i-1])return a->words[i-1]>b->words[i-1]?1:-1;
    return 0;
}
static unsigned bits(const Big *a) {
    if(!a->count)return 0;
    uint32_t n=a->words[a->count-1];unsigned high=0;
    while(n){high++;n>>=1;}
    return (unsigned)((a->count-1)*32)+high;
}
static bool shift(Big *a,unsigned amount) {
    if(!a->count||!amount)return true;
    size_t whole=amount/32;unsigned part=amount%32;
    if(a->count+whole+(part?1u:0u)>BIG_WORDS)return false;
    size_t previous=a->count;
    for(size_t i=previous;i>0;i--)a->words[i-1+whole]=a->words[i-1];
    for(size_t i=0;i<whole;i++)a->words[i]=0;
    a->count+=whole;
    if(part) {
        uint32_t carry=0;
        for(size_t i=whole;i<a->count;i++) {
            uint32_t word=a->words[i];a->words[i]=(word<<part)|carry;carry=word>>(32-part);
        }
        if(carry)a->words[a->count++]=carry;
    }
    return true;
}
static void subtract(Big *a,const Big *b) {
    uint64_t borrow=0;
    for(size_t i=0;i<a->count;i++) {
        uint64_t remove=(i<b->count?b->words[i]:0)+borrow;
        uint64_t value=a->words[i];a->words[i]=(uint32_t)(value-remove);borrow=value<remove;
    }
    while(a->count&&!a->words[a->count-1])a->count--;
}
static bool rational(Big numerator,Big denominator,bool sticky,uint64_t *out) {
    int exponent=(int)bits(&numerator)-(int)bits(&denominator);
    Big aligned=exponent>=0?denominator:numerator;
    if(!shift(&aligned,(unsigned)(exponent>=0?exponent:-exponent)))return false;
    if((exponent>=0?compare(&numerator,&aligned):compare(&aligned,&denominator))<0)exponent--;
    if(exponent>1023){*out=UINT64_C(0x7ff0000000000000);return true;}
    if(exponent<-1075){*out=0;return true;}
    int scale=exponent>=-1022?52-exponent:1074;
    if(scale>=0) {if(!shift(&numerator,(unsigned)scale))return false;}
    else if(!shift(&denominator,(unsigned)-scale))return false;
    int quotientBits=(int)bits(&numerator)-(int)bits(&denominator);
    uint64_t q=0;
    for(int i=quotientBits;i>=0;i--) {
        aligned=denominator;
        if(i>=64||!shift(&aligned,(unsigned)i))return false;
        if(compare(&numerator,&aligned)>=0){subtract(&numerator,&aligned);q|=UINT64_C(1)<<(unsigned)i;}
    }
    if(!shift(&numerator,1))return false;
    int half=compare(&numerator,&denominator);
    if(half>0||(half==0&&((q&1)||sticky)))q++;
    if(exponent<-1022){*out=q;return true;}
    if(q==UINT64_C(0x20000000000000)) {q>>=1;exponent++;}
    if(exponent>1023){*out=UINT64_C(0x7ff0000000000000);return true;}
    *out=((uint64_t)(exponent+1023)<<52)|(q&UINT64_C(0xfffffffffffff));return true;
}
static bool equalsAscii(const uint16_t *u,size_t begin,size_t end,const char *s) {
    size_t n=strlen(s);if(end-begin!=n)return false;
    for(size_t i=0;i<n;i++)if(u[begin+i]!=(uint16_t)(unsigned char)s[i])return false;
    return true;
}
static bool exponentValue(const uint16_t *u,size_t end,size_t *position,int64_t *out) {
    size_t i=*position;bool minus=false;
    if(i<end&&(u[i]=='+'||u[i]=='-')){minus=u[i]=='-';i++;}
    size_t start=i;int64_t n=0;
    while(i<end&&u[i]>='0'&&u[i]<='9') {
        if(n<INT64_C(1000000000000))n=n*10+(u[i]-'0');
        i++;
    }
    if(i==start)return false;
    *position=i;*out=minus?-n:n;return true;
}
static int hexDigit(uint16_t c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
NativeJavaNumberResult NativeJavaNumber_parseDouble(const NBTString *s,double *out) {
    if(!out||!valid(s)||sizeof(double)!=sizeof(uint64_t))return NATIVE_NUMBER_FAILURE;
    if(!s)return NATIVE_NUMBER_NULL_POINTER;
    double one=1.0;uint64_t raw;memcpy(&raw,&one,sizeof raw);
    if(raw!=UINT64_C(0x3ff0000000000000))return NATIVE_NUMBER_FAILURE;
    size_t i=0,end=s->length;const uint16_t *u=s->units;
    while(i<end&&u[i]<=0x20)i++;
    while(end>i&&u[end-1]<=0x20)end--;
    if(i==end)return NATIVE_NUMBER_FORMAT;
    bool negative=false;
    if(u[i]=='+'||u[i]=='-'){negative=u[i]=='-';i++;}
    if(i==end)return NATIVE_NUMBER_FORMAT;
    if(equalsAscii(u,i,end,"NaN")){raw=UINT64_C(0x7ff8000000000000);goto success;}
    if(equalsAscii(u,i,end,"Infinity")){raw=UINT64_C(0x7ff0000000000000);goto sign;}
    bool hex=end-i>=2&&u[i]=='0'&&(u[i+1]=='x'||u[i+1]=='X');
    if(hex)i+=2;
    Big number,denominator;small(&number,0);small(&denominator,1);
    bool point=false,seen=false,started=false,sticky=false;
    size_t kept=0,discarded=0,fractional=0;
    for(;i<end;i++) {
        if(u[i]=='.'&&!point){point=true;continue;}
        int digit=hex?hexDigit(u[i]):(u[i]>='0'&&u[i]<='9'?(int)(u[i]-'0'):-1);
        if(digit<0)break;
        seen=true;if(point)fractional++;
        if(!started&&!digit)continue;
        started=true;
        if(kept<(hex?HEX_KEEP:DECIMAL_KEEP)) {
            if(!multiply(&number,hex?16u:10u,(uint32_t)digit))return NATIVE_NUMBER_FAILURE;
            kept++;
        }else {discarded++;if(digit)sticky=true;}
    }
    if(!seen)return NATIVE_NUMBER_FORMAT;
    int64_t exponent=0;
    if(i<end&&((hex&&(u[i]=='p'||u[i]=='P'))||(!hex&&(u[i]=='e'||u[i]=='E')))) {
        i++;if(!exponentValue(u,end,&i,&exponent))return NATIVE_NUMBER_FORMAT;
    }else if(hex)return NATIVE_NUMBER_FORMAT;
    if(i<end&&(u[i]=='f'||u[i]=='F'||u[i]=='d'||u[i]=='D'))i++;
    if(i!=end)return NATIVE_NUMBER_FORMAT;
    if(!started){raw=0;goto sign;}
    if(hex) {
        exponent+=(int64_t)discarded*4-(int64_t)fractional*4;
        int64_t order=exponent+(int64_t)bits(&number)-1;
        if(order>1023){raw=UINT64_C(0x7ff0000000000000);goto sign;}
        if(order<-1075){raw=0;goto sign;}
        if(exponent>=0) {if(!shift(&number,(unsigned)exponent))return NATIVE_NUMBER_FAILURE;}
        else if(!shift(&denominator,(unsigned)-exponent))return NATIVE_NUMBER_FAILURE;
    }else {
        exponent+=(int64_t)discarded-(int64_t)fractional;
        int64_t order=exponent+(int64_t)kept;
        if(order>310){raw=UINT64_C(0x7ff0000000000000);goto sign;}
        if(order<-325){raw=0;goto sign;}
        if(exponent>=0) {
            for(int64_t k=0;k<exponent;k++)if(!multiply(&number,10,0))return NATIVE_NUMBER_FAILURE;
        }else for(int64_t k=0;k<-exponent;k++)if(!multiply(&denominator,10,0))return NATIVE_NUMBER_FAILURE;
    }
    if(!rational(number,denominator,sticky,&raw))return NATIVE_NUMBER_FAILURE;
sign:
    if(negative)raw|=UINT64_C(0x8000000000000000);
success:
    memcpy(out,&raw,sizeof raw);return NATIVE_NUMBER_OK;
}
