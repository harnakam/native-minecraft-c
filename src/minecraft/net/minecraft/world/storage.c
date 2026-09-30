#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
static bool write_u32(FILE *f, uint32_t n) {
    unsigned char b[4] = {(unsigned char)n,(unsigned char)(n>>8),(unsigned char)(n>>16),(unsigned char)(n>>24)};
    return fwrite(b,1,4,f)==4;
}
static bool read_u32(FILE *f, uint32_t *n) {
    unsigned char b[4]; if (fread(b,1,4,f)!=4) return false;
    *n = (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24); return true;
}
static bool write_state(FILE *f, uint16_t n) {
    unsigned char b[2] = {(unsigned char)n,(unsigned char)(n>>8)}; return fwrite(b,1,2,f)==2;
}
static bool read_state(FILE *f, uint16_t *n) {
    unsigned char b[2]; if (fread(b,1,2,f)!=2) return false;
    *n = (uint16_t)((unsigned)b[0]|((unsigned)b[1]<<8)); return true;
}
static void set_error(char *out, unsigned size, const char *msg) { if (size) snprintf(out,size,"%s",msg); }
bool mc_world_save(const mc_world *w, const char *path, char *error, unsigned size) {
    if (!path || !*path || strlen(path)>3900) { set_error(error,size,"Invalid save path"); return false; }
    char temp[4096]; snprintf(temp,sizeof(temp),"%s.tmp",path);
    FILE *f = fopen(temp,"wb");
    if (!f) { set_error(error,size,"Cannot open temporary save; check parent directory and permissions"); return false; }
    bool ok = fwrite("C919WRL1",1,8,f)==8 && write_u32(f,w->seed) && write_u32(f,(uint32_t)w->count);
    for (int i = 0; ok && i < w->count; ++i) {
        const mc_chunk *c = &w->chunks[i]; uint32_t runs = 0;
        for (size_t j=0; j<MC_CHUNK_BLOCKS; ++runs) { uint16_t state=c->blocks[j++]; while (j<MC_CHUNK_BLOCKS && c->blocks[j]==state) ++j; }
        ok = write_u32(f,(uint32_t)c->x) && write_u32(f,(uint32_t)c->z) && write_u32(f,runs);
        for (size_t j=0; ok && j<MC_CHUNK_BLOCKS;) {
            size_t start=j; uint16_t state=c->blocks[j++]; while (j<MC_CHUNK_BLOCKS && c->blocks[j]==state) ++j;
            ok = write_u32(f,(uint32_t)(j-start)) && write_state(f,state);
        }
    }
    if (fflush(f)!=0) ok=false;
#ifdef _WIN32
    if (ok && _commit(_fileno(f))!=0) ok=false;
#else
    if (ok && fsync(fileno(f))!=0) ok=false;
#endif
    if (fclose(f)!=0) ok=false;
    if (ok) {
#ifdef _WIN32
        ok = MoveFileExA(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        ok = rename(temp,path)==0;
#endif
    }
    if (!ok) { remove(temp); set_error(error,size,"Save failed; previous committed world is preserved"); }
    else set_error(error,size,"");
    return ok;
}
bool mc_world_load(mc_world *w, const char *path, char *error, unsigned size) {
    FILE *f = fopen(path,"rb");
    if (!f) { set_error(error,size,"Cannot open world"); return false; }
    mc_world loaded; mc_world_init(&loaded,0);
    char magic[8]; uint32_t count=0;
    bool ok = fread(magic,1,8,f)==8 && !memcmp(magic,"C919WRL1",8) && read_u32(f,&loaded.seed) && read_u32(f,&count) && count<=MC_MAX_CHUNKS;
    for (uint32_t i=0; ok && i<count; ++i) {
        uint32_t x,z,runs; ok = read_u32(f,&x) && read_u32(f,&z) && read_u32(f,&runs) && runs>0 && runs<=MC_CHUNK_BLOCKS;
        if (!ok) break;
        if (mc_world_chunk(&loaded,(int32_t)x,(int32_t)z,false)) { ok=false; break; }
        mc_chunk *c = mc_world_chunk(&loaded,(int32_t)x,(int32_t)z,true);
        if (!c) { ok=false; break; }
        size_t offset=0;
        for (uint32_t r=0; ok && r<runs; ++r) {
            uint32_t length; uint16_t state;
            ok = read_u32(f,&length) && read_state(f,&state) && length>0 && length<=MC_CHUNK_BLOCKS-offset;
            if (!ok) break;
            for (uint32_t j=0; j<length; ++j) c->blocks[offset++] = state;
        }
        if (offset!=MC_CHUNK_BLOCKS) ok=false;
    }
    if (ok && fgetc(f)!=EOF) ok=false;
    if (ferror(f)) ok=false;
    fclose(f);
    if (!ok) { mc_world_free(&loaded); set_error(error,size,"Invalid or truncated C919 world; active world was preserved"); return false; }
    mc_world_free(w); *w=loaded; set_error(error,size,""); return true;
}
