#include "nbt/nbt.h"
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr,"CHECK failed: %s at %s:%d\n",#condition,__FILE__,__LINE__); exit(EXIT_FAILURE); } } while (0)
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define make_dir(path) mkdir(path,0700)
#endif

static void named(mc_buf *b,uint8_t type,const char *name) {
    mc_put_u8(b,type); mc_put_i16(b,(int16_t)strlen(name)); mc_put_bytes(b,name,strlen(name));
}
static void text_value(mc_buf *b,const char *s) {
    mc_put_i16(b,(int16_t)strlen(s)); mc_put_bytes(b,s,strlen(s));
}
static void fixture(mc_buf *b) {
    named(b,10,"book");
    named(b,1,"byte"); mc_put_u8(b,0x80);
    named(b,2,"short"); mc_put_i16(b,-1234);
    named(b,3,"int"); mc_put_i32(b,INT32_MIN);
    named(b,4,"long"); mc_put_i64(b,INT64_MIN);
    named(b,5,"float"); mc_put_f32(b,1.25f);
    named(b,6,"double"); mc_put_f64(b,-3.5);
    named(b,7,"opaque"); mc_put_i32(b,4); mc_put_bytes(b,"\0\xff\x80\x7f",4);
    named(b,8,"author"); text_value(b,"Player");
    named(b,9,"pages"); mc_put_u8(b,8); mc_put_i32(b,2);
    text_value(b,"{\"text\":\"Page one\"}"); text_value(b,"Second page");
    named(b,9,"ench"); mc_put_u8(b,10); mc_put_i32(b,2);
    named(b,2,"id"); mc_put_i16(b,16); named(b,2,"lvl"); mc_put_i16(b,5); mc_put_u8(b,0);
    named(b,2,"id"); mc_put_i16(b,34); named(b,2,"lvl"); mc_put_i16(b,3); mc_put_u8(b,0);
    named(b,10,"display"); named(b,8,"Name"); text_value(b,"A book"); mc_put_u8(b,0);
    named(b,11,"unknown"); mc_put_i32(b,3); mc_put_i32(b,INT32_MIN); mc_put_i32(b,0); mc_put_i32(b,INT32_MAX);
    named(b,9,"empty"); mc_put_u8(b,0); mc_put_i32(b,0);
    mc_put_u8(b,0); CHECK(!b->failed);
}
static mc_nbt read_fixture(const mc_buf *bytes) {
    mc_nbt result; mc_nbt_init(&result);
    mc_buf in=*bytes; in.pos=0;
    CHECK(mc_nbt_read(&in,&result)); CHECK(in.pos==in.len); CHECK(!in.failed);
    return result;
}
static void test_preservation_and_views(void) {
    mc_buf b; mc_buf_init(&b); fixture(&b);
    CHECK(mc_nbt_validate(b.data,b.len));
    mc_nbt n=read_fixture(&b),copy; mc_nbt_init(&copy);
    CHECK(n.size==b.len && !memcmp(n.data,b.data,b.len));
    CHECK(mc_nbt_copy(&copy,&n)); CHECK(mc_nbt_equal(&copy,&n)); CHECK(copy.data!=n.data);
    CHECK(mc_nbt_copy(&copy,&copy));
    mc_buf encoded; mc_buf_init(&encoded); CHECK(mc_nbt_write(&encoded,&n));
    CHECK(encoded.len==b.len && !memcmp(encoded.data,b.data,b.len));
    mc_nbt_view root,view,element;
    CHECK(mc_nbt_root(&n,&root)); CHECK(root.type==10);
    const char *integer_names[]={"byte","short","int","long"};
    const int64_t integer_values[]={-128,-1234,INT32_MIN,INT64_MIN};
    for (size_t i=0;i<4;i++) {
        int64_t v=0; double d=0;
        CHECK(mc_nbt_find(&root,integer_names[i],&view));
        CHECK(mc_nbt_get_integer(&view,&v) && v==integer_values[i]);
        CHECK(mc_nbt_get_number(&view,&d) && d==(double)integer_values[i]);
    }
    double d; int64_t v=91;
    CHECK(mc_nbt_find(&root,"float",&view)); CHECK(mc_nbt_get_number(&view,&d) && d==1.25);
    CHECK(!mc_nbt_get_integer(&view,&v) && v==91);
    CHECK(mc_nbt_find(&root,"double",&view)); CHECK(mc_nbt_get_number(&view,&d) && d==-3.5);
    CHECK(mc_nbt_find(&root,"unknown",&view)); CHECK(view.type==11 && view.size==16);
    CHECK(mc_nbt_find(&root,"opaque",&view)); CHECK(view.type==7 && view.size==8 && view.data[5]==255);
    CHECK(mc_nbt_find(&root,"pages",&view));
    char s[64]; CHECK(mc_nbt_list_get(&view,1,&element));
    CHECK(mc_nbt_get_string(&element,s,sizeof(s)) && !strcmp(s,"Second page"));
    mc_nbt_view unchanged=element;
    CHECK(!mc_nbt_list_get(&view,2,&element)); CHECK(!memcmp(&unchanged,&element,sizeof(element)));
    CHECK(mc_nbt_find(&root,"ench",&view)); CHECK(mc_nbt_list_get(&view,1,&element));
    CHECK(mc_nbt_find(&element,"id",&view)); CHECK(mc_nbt_get_integer(&view,&v) && v==34);
    CHECK(mc_nbt_find(&root,"display",&view)); CHECK(mc_nbt_find(&view,"Name",&element));
    CHECK(mc_nbt_get_string(&element,s,sizeof(s)) && !strcmp(s,"A book"));
    strcpy(s,"unchanged"); CHECK(!mc_nbt_get_string(&element,s,3)); CHECK(!strcmp(s,"unchanged"));
    CHECK(!mc_nbt_find(&root,"missing",&element));
    mc_nbt invalid={(uint8_t *)"\xff",1};
    CHECK(!mc_nbt_copy(&copy,&invalid)); CHECK(mc_nbt_equal(&copy,&n));
    CHECK(!mc_nbt_write(&encoded,&invalid) && encoded.failed);
    mc_nbt_free(&copy); mc_nbt_free(&n); mc_buf_free(&encoded); mc_buf_free(&b);
}
static void test_roots_and_transactionality(void) {
    const uint8_t int_root[]={3,0,1,'x',0x12,0x34,0x56,0x78};
    mc_buf stream; mc_buf_init(&stream); mc_put_bytes(&stream,int_root,sizeof(int_root));
    mc_put_u8(&stream,0); mc_put_u8(&stream,99);
    mc_nbt n; mc_nbt_init(&n); CHECK(mc_nbt_read(&stream,&n)); CHECK(stream.pos==8);
    mc_nbt_view root; int64_t value;
    CHECK(mc_nbt_root(&n,&root)); CHECK(mc_nbt_get_integer(&root,&value) && value==305419896);
    CHECK(mc_nbt_read(&stream,&n)); CHECK(n.data==NULL && n.size==0 && stream.pos==9);
    CHECK(mc_nbt_root(&n,&root) && root.type==0 && root.size==0);
    CHECK(!mc_nbt_read(&stream,&n)); CHECK(stream.failed && stream.pos==9 && n.size==0);
    mc_buf encoded; mc_buf_init(&encoded); CHECK(mc_nbt_write(&encoded,&n));
    CHECK(encoded.len==1 && encoded.data[0]==0);
    CHECK(mc_nbt_validate(encoded.data,encoded.len)); CHECK(!mc_nbt_validate(NULL,0));
    CHECK(!mc_nbt_validate(stream.data,stream.len));
    mc_buf b; mc_buf_init(&b); fixture(&b);
    mc_nbt good=read_fixture(&b); CHECK(mc_nbt_copy(&n,&good));
    for (size_t length=0;length<b.len;length++) {
        CHECK(!mc_nbt_validate(b.data,length));
        mc_buf in=b; in.len=length; in.pos=0;
        CHECK(!mc_nbt_read(&in,&n)); CHECK(in.failed && in.pos==0 && mc_nbt_equal(&n,&good));
    }
    mc_buf in=b; in.failed=true; CHECK(!mc_nbt_read(&in,&n) && in.pos==0);
    mc_nbt_free(&good); mc_nbt_free(&n); mc_buf_free(&b); mc_buf_free(&stream); mc_buf_free(&encoded);
}
static void test_malformed(void) {
    const uint8_t bad[][12]={
        {12,0,0}, {7,0,0,255,255,255,255}, {11,0,0,127,255,255,255},
        {9,0,0,0,0,0,0,1}, {9,0,0,12,0,0,0,0}, {9,0,0,1,255,255,255,255},
        {8,0,0,0,1,0}, {8,0,0,0,2,0xc1,0x81}, {8,0,0,0,1,0x80},
        {8,0,0,0,4,0xf0,0x9f,0x98,0x80}, {10,0,0,0,0}
    };
    const size_t sizes[]={3,7,7,8,8,8,6,7,6,9,5};
    for (size_t i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++) CHECK(!mc_nbt_validate(bad[i],sizes[i]));
    mc_buf depth; mc_buf_init(&depth);
    for (unsigned i=0;i<64;i++) named(&depth,10,"");
    for (unsigned i=0;i<64;i++) mc_put_u8(&depth,0);
    CHECK(mc_nbt_validate(depth.data,depth.len));
    mc_buf_clear(&depth);
    for (unsigned i=0;i<65;i++) named(&depth,10,"");
    for (unsigned i=0;i<65;i++) mc_put_u8(&depth,0);
    CHECK(!mc_nbt_validate(depth.data,depth.len)); mc_buf_free(&depth);
    uint8_t tiny=0; CHECK(!mc_nbt_validate(&tiny,MC_NBT_MAX_BYTES+1u));
    const uint8_t list[]={1,0,0,0,2,127};
    mc_nbt_view malformed={9,list,sizeof(list)}; mc_nbt_view out={1,&tiny,1},previous=out;
    CHECK(!mc_nbt_list_get(&malformed,0,&out)); CHECK(!memcmp(&out,&previous,sizeof(out)));
    mc_nbt_view byte={1,&tiny,2}; int64_t value=8;
    CHECK(!mc_nbt_get_integer(&byte,&value) && value==8);
}
static void test_modified_utf8(void) {
    /* U+1F600 is the UTF-16 surrogate pair D83D DE00 in DataInput modified UTF-8. */
    const uint8_t encoded[]={10,0,0,8,0,6,0xed,0xa0,0xbd,0xed,0xb8,0x80,0,9,
                             'A',0xc2,0xa2,0xed,0xa0,0xbd,0xed,0xb8,0x80,0};
    CHECK(mc_nbt_validate(encoded,sizeof(encoded)));
    mc_buf input; mc_buf_init(&input); mc_put_bytes(&input,encoded,sizeof(encoded));
    mc_nbt n=read_fixture(&input); mc_nbt_view root,view;
    CHECK(mc_nbt_root(&n,&root)); CHECK(mc_nbt_find(&root,"\xf0\x9f\x98\x80",&view));
    char output[32]; CHECK(mc_nbt_get_string(&view,output,sizeof(output)));
    CHECK(!strcmp(output,"A\xc2\xa2\xf0\x9f\x98\x80"));
    const uint8_t nul[]={0,2,0xc0,0x80}; mc_nbt_view nul_view={8,nul,sizeof(nul)};
    const uint8_t surrogate[]={0,3,0xed,0xa0,0xbd}; mc_nbt_view surrogate_view={8,surrogate,sizeof(surrogate)};
    strcpy(output,"untouched"); CHECK(!mc_nbt_get_string(&nul_view,output,sizeof(output)));
    CHECK(!strcmp(output,"untouched")); CHECK(!mc_nbt_get_string(&surrogate_view,output,sizeof(output)));
    uint8_t alias[16]={0,5,'H','e','l','l','o'}; mc_nbt_view alias_view={8,alias,7};
    CHECK(mc_nbt_get_string(&alias_view,(char *)alias+3,sizeof(alias)-3));
    CHECK(!strcmp((char *)alias+3,"Hello"));
    mc_nbt_free(&n); mc_buf_free(&input);
}
static void test_compound_equality(void) {
    mc_buf a,b; mc_buf_init(&a); mc_buf_init(&b);
    named(&a,10,"first root"); named(&a,1,"a"); mc_put_u8(&a,1);
    named(&a,10,"nested"); named(&a,2,"b"); mc_put_i16(&a,2); named(&a,1,"a"); mc_put_u8(&a,3);
    mc_put_u8(&a,0); mc_put_u8(&a,0);
    named(&b,10,"second root"); named(&b,10,"nested"); named(&b,1,"a"); mc_put_u8(&b,3);
    named(&b,2,"b"); mc_put_i16(&b,2); mc_put_u8(&b,0);
    named(&b,1,"a"); mc_put_u8(&b,99); named(&b,1,"a"); mc_put_u8(&b,1); mc_put_u8(&b,0);
    mc_nbt left=read_fixture(&a),right=read_fixture(&b);
    CHECK(mc_nbt_equal(&left,&right)); CHECK(mc_nbt_equal(&right,&left));
    right.data[right.size-2]=2; CHECK(!mc_nbt_equal(&left,&right));
    mc_nbt_free(&left); mc_nbt_free(&right); mc_buf_free(&a); mc_buf_free(&b);
}
static mc_nbt floating_root(uint8_t type,uint64_t bits,bool compound) {
    mc_buf encoded; mc_buf_init(&encoded);
    if (compound) named(&encoded,10,"");
    named(&encoded,type,"value");
    if (type==5) mc_put_i32(&encoded,(int32_t)(uint32_t)bits);
    else mc_put_i64(&encoded,(int64_t)bits);
    if (compound) mc_put_u8(&encoded,0);
    mc_nbt result=read_fixture(&encoded); mc_buf_free(&encoded); return result;
}
static void test_floating_equality(void) {
    for (uint8_t type=5;type<=6;type++) {
        uint64_t sign=type==5 ? UINT64_C(0x80000000) : UINT64_C(0x8000000000000000);
        uint64_t inf=type==5 ? UINT64_C(0x7f800000) : UINT64_C(0x7ff0000000000000);
        uint64_t nan=type==5 ? UINT64_C(0x7fc00000) : UINT64_C(0x7ff8000000000000);
        mc_nbt positive=floating_root(type,0,false),negative=floating_root(type,sign,false);
        mc_nbt infinity=floating_root(type,inf,false),same_infinity=floating_root(type,inf,false);
        mc_nbt negative_infinity=floating_root(type,inf|sign,false);
        mc_nbt first_nan=floating_root(type,nan,false),second_nan=floating_root(type,nan,false);
        CHECK(mc_nbt_equal(&positive,&negative) && mc_nbt_equal(&negative,&positive));
        CHECK(mc_nbt_equal(&infinity,&same_infinity) && !mc_nbt_equal(&infinity,&negative_infinity));
        CHECK(!mc_nbt_equal(&first_nan,&second_nan) && !mc_nbt_equal(&first_nan,&first_nan));
        /* Equality does not canonicalize stored IEEE bytes. */
        CHECK(positive.size==negative.size && memcmp(positive.data,negative.data,positive.size));
        mc_nbt_free(&positive); mc_nbt_free(&negative); mc_nbt_free(&infinity);
        mc_nbt_free(&same_infinity); mc_nbt_free(&negative_infinity); mc_nbt_free(&first_nan); mc_nbt_free(&second_nan);
        mc_nbt a=floating_root(type,nan,true),b=floating_root(type,nan,true);
        CHECK(mc_nbt_equal(&a,&a) && !mc_nbt_equal(&a,&b)); mc_nbt_free(&a); mc_nbt_free(&b);
        mc_buf list_bytes; mc_buf_init(&list_bytes); named(&list_bytes,9,"list");
        mc_put_u8(&list_bytes,type); mc_put_i32(&list_bytes,1);
        if (type==5) mc_put_i32(&list_bytes,(int32_t)(uint32_t)nan);
        else mc_put_i64(&list_bytes,(int64_t)nan);
        mc_nbt list=read_fixture(&list_bytes),copy; mc_nbt_init(&copy);
        CHECK(mc_nbt_copy(&copy,&list));
        CHECK(mc_nbt_equal(&list,&list) && !mc_nbt_equal(&list,&copy));
        mc_nbt_free(&list); mc_nbt_free(&copy); mc_buf_free(&list_bytes);
    }
    mc_nbt f=floating_root(5,0,false),d=floating_root(6,0,false);
    CHECK(!mc_nbt_equal(&f,&d)); mc_nbt_free(&f); mc_nbt_free(&d);
}
static uint8_t *file_read(const char *path,size_t *size) {
    FILE *f=fopen(path,"rb"); CHECK(f); CHECK(fseek(f,0,SEEK_END)==0); long len=ftell(f); CHECK(len>=0);
    rewind(f); uint8_t *data=malloc((size_t)len+1); CHECK(data);
    CHECK(fread(data,1,(size_t)len,f)==(size_t)len); CHECK(fclose(f)==0); *size=(size_t)len; return data;
}
static void file_write(const char *path,const void *data,size_t size) {
    FILE *f=fopen(path,"wb"); CHECK(f); CHECK(fwrite(data,1,size,f)==size); CHECK(fclose(f)==0);
}
static void test_gzip_persistence(void) {
    (void)make_dir(".local"); (void)make_dir(".local/nbt-tests");
    const char *path=".local/nbt-tests/roundtrip.nbt.gz",*bad_path=".local/nbt-tests/corrupt.nbt.gz";
    char error[192]; mc_buf b; mc_buf_init(&b); fixture(&b);
    mc_nbt n=read_fixture(&b),loaded; mc_nbt_init(&loaded);
    CHECK(mc_nbt_save_gzip(&n,path,error,sizeof(error)) && !error[0]);
    CHECK(mc_nbt_load_gzip(&loaded,path,error,sizeof(error)) && !error[0]); CHECK(mc_nbt_equal(&loaded,&n));
    size_t size; uint8_t *compressed=file_read(path,&size); CHECK(size>18);
    for (size_t length=0;length<size;length++) {
        file_write(bad_path,compressed,length);
        CHECK(!mc_nbt_load_gzip(&loaded,bad_path,error,sizeof(error)) && error[0]); CHECK(mc_nbt_equal(&loaded,&n));
    }
    compressed[size-8]^=1; file_write(bad_path,compressed,size);
    CHECK(!mc_nbt_load_gzip(&loaded,bad_path,error,sizeof(error))); CHECK(mc_nbt_equal(&loaded,&n)); compressed[size-8]^=1;
    compressed[3]|=0x20; file_write(bad_path,compressed,size);
    CHECK(!mc_nbt_load_gzip(&loaded,bad_path,error,sizeof(error))); compressed[3]&=(uint8_t)~0x20;
    uint8_t *trailing=malloc(size+1); CHECK(trailing); memcpy(trailing,compressed,size); trailing[size]=0;
    file_write(bad_path,trailing,size+1); CHECK(!mc_nbt_load_gzip(&loaded,bad_path,error,sizeof(error)));
    uint8_t *concatenated=malloc(size*2); CHECK(concatenated); memcpy(concatenated,compressed,size); memcpy(concatenated+size,compressed,size);
    file_write(bad_path,concatenated,size*2); CHECK(!mc_nbt_load_gzip(&loaded,bad_path,error,sizeof(error)));
    mc_nbt invalid={(uint8_t *)"\xff",1}; CHECK(!mc_nbt_save_gzip(&invalid,path,error,sizeof(error)));
    size_t after_size; uint8_t *after=file_read(path,&after_size); CHECK(after_size==size && !memcmp(after,compressed,size)); free(after);
    CHECK(!mc_nbt_save_gzip(&n,".local/nbt-tests",error,sizeof(error)) && error[0]);
    CHECK(!mc_nbt_save_gzip(&n,".local/nbt-tests/missing/path.nbt.gz",error,sizeof(error)) && error[0]);
    CHECK(!mc_nbt_load_gzip(&loaded,".local/nbt-tests/missing.nbt.gz",error,sizeof(error)) && error[0]);
    /* A valid gzip stream still cannot exceed the inflated NBT size cap. */
    size_t oversized=MC_NBT_MAX_BYTES+1u; uint8_t *bomb=calloc(1,oversized); CHECK(bomb);
    gzFile gz=gzopen(bad_path,"wb"); CHECK(gz); CHECK(gzwrite(gz,bomb,(unsigned)oversized)==(int)oversized); CHECK(gzclose(gz)==Z_OK);
    CHECK(!mc_nbt_load_gzip(&loaded,bad_path,error,sizeof(error))); CHECK(mc_nbt_equal(&loaded,&n));
    free(bomb); free(concatenated); free(trailing); free(compressed);
    mc_nbt_free(&loaded); mc_nbt_free(&n); mc_buf_free(&b);
    CHECK(remove(path)==0); CHECK(remove(bad_path)==0);
}
int main(void) {
    test_preservation_and_views(); test_roots_and_transactionality(); test_malformed();
    test_modified_utf8(); test_compound_equality(); test_floating_equality(); test_gzip_persistence(); puts("NBT tests passed"); return 0;
}
