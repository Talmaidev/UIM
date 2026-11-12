#include "uim_component_manager.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* ---- UTF-8 路径文件桥 ----
 * Windows 的 fopen / MoveFileExA / remove 都按 ANSI 代码页解释路径，
 * UTF-8 路径（中文、日文、emoji 目录）会打不开或写错位置。
 * 这里统一转宽字符后调 W 版 API；POSIX 直接吃 UTF-8 字节，无转换。 */
#if defined(_WIN32)
#define UIM_WPATH_CAP 1024

static bool uim_utf8_to_wide(const char* utf8, wchar_t* out, size_t cap) {
    int n;
    if (!utf8 || !out || cap == 0) return false;
    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, (int)cap);
    return n > 0;
}

static FILE* uim_fopen_utf8(const char* utf8_path, const char* mode) {
    wchar_t wpath[UIM_WPATH_CAP];
    wchar_t wmode[16];
    size_t i;
    if (!uim_utf8_to_wide(utf8_path, wpath, UIM_WPATH_CAP)) return NULL;
    for (i = 0; i + 1 < sizeof(wmode) / sizeof(wmode[0]) && mode[i]; i++) {
        wmode[i] = (wchar_t)(unsigned char)mode[i];
    }
    wmode[i] = L'\0';
    return _wfopen(wpath, wmode);
}

static bool uim_move_utf8(const char* from, const char* to) {
    wchar_t wfrom[UIM_WPATH_CAP], wto[UIM_WPATH_CAP];
    if (!uim_utf8_to_wide(from, wfrom, UIM_WPATH_CAP)) return false;
    if (!uim_utf8_to_wide(to, wto, UIM_WPATH_CAP)) return false;
    return MoveFileExW(wfrom, wto, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

static void uim_remove_utf8(const char* utf8_path) {
    wchar_t wpath[UIM_WPATH_CAP];
    if (uim_utf8_to_wide(utf8_path, wpath, UIM_WPATH_CAP)) (void)DeleteFileW(wpath);
}
#else
static FILE* uim_fopen_utf8(const char* utf8_path, const char* mode) {
    return fopen(utf8_path, mode);
}

static bool uim_move_utf8(const char* from, const char* to) {
    return rename(from, to) == 0;
}

static void uim_remove_utf8(const char* utf8_path) {
    (void)remove(utf8_path);
}
#endif

#define FILE_VERSION 1u
#define FILE_LIMIT (64u * 1024u * 1024u)

typedef struct { unsigned char *p; size_t n, cap; } Writer;
typedef struct { const unsigned char *p; size_t n, at; } Reader;
typedef struct { char *key, *parent; uim_NodeId id; uim_Rect rect; bool visible, enabled; int32_t z; uint32_t order; } LayoutRecord;
typedef struct { char *key; uim_PropertyId id; uim_Value value; } PresetRecord;

static bool reserve(Writer *w, size_t add) {
    size_t need, cap;
    unsigned char *p;
    if (add > SIZE_MAX - w->n) return false;
    need = w->n + add;
    if (need <= w->cap) return true;
    cap = w->cap ? w->cap : 256;
    while (cap < need) { if (cap > SIZE_MAX / 2) return false; cap *= 2; }
    p = (unsigned char *)realloc(w->p, cap);
    if (!p) return false;
    w->p = p; w->cap = cap; return true;
}
static bool put_bytes(Writer *w, const void *p, size_t n) { if (!reserve(w,n)) return false; if(n)memcpy(w->p+w->n,p,n);w->n+=n;return true; }
static bool put_u8(Writer*w,uint8_t v){return put_bytes(w,&v,1);}
static bool put_u32(Writer*w,uint32_t v){unsigned char b[4]={(unsigned char)v,(unsigned char)(v>>8),(unsigned char)(v>>16),(unsigned char)(v>>24)};return put_bytes(w,b,4);}
static bool put_u64(Writer*w,uint64_t v){return put_u32(w,(uint32_t)v)&&put_u32(w,(uint32_t)(v>>32));}
static bool put_f32(Writer*w,float v){uint32_t u;memcpy(&u,&v,4);return put_u32(w,u);}
static bool put_f64(Writer*w,double v){uint64_t u;memcpy(&u,&v,8);return put_u64(w,u);}
static bool put_string(Writer*w,const char*s){size_t n;if(!s)return false;n=strlen(s);return n<=UINT32_MAX&&put_u32(w,(uint32_t)n)&&put_bytes(w,s,n);}
static bool get_bytes(Reader*r,void*out,size_t n){if(n>r->n-r->at)return false;if(n)memcpy(out,r->p+r->at,n);r->at+=n;return true;}
static bool get_u8(Reader*r,uint8_t*v){return get_bytes(r,v,1);}
static bool get_u32(Reader*r,uint32_t*v){unsigned char b[4];if(!get_bytes(r,b,4))return false;*v=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);return true;}
static bool get_u64(Reader*r,uint64_t*v){uint32_t a,b;if(!get_u32(r,&a)||!get_u32(r,&b))return false;*v=(uint64_t)a|((uint64_t)b<<32);return true;}
static bool get_f32(Reader*r,float*v){uint32_t u;if(!get_u32(r,&u))return false;memcpy(v,&u,4);return isfinite(*v);}
static bool get_f64(Reader*r,double*v){uint64_t u;if(!get_u64(r,&u))return false;memcpy(v,&u,8);return isfinite(*v);}
static bool get_string(Reader*r,char**out){uint32_t n;char*s;if(!get_u32(r,&n)||n>FILE_LIMIT||n>r->n-r->at)return false;s=(char*)malloc((size_t)n+1);if(!s)return false;if(n)memcpy(s,r->p+r->at,n);s[n]='\0';r->at+=n;if(memchr(s,'\0',n)){free(s);return false;}*out=s;return true;}
static uint32_t hash32(const unsigned char*p,size_t n){uint32_t h=2166136261u;size_t i;for(i=0;i<n;i++){h^=p[i];h*=16777619u;}return h;}
static bool finish_file(Writer*w){uint32_t h=hash32(w->p,w->n);return put_u32(w,h);}
static bool validate_file(Reader*r,const char magic[4]){uint32_t ver,stored;if(r->n<12||memcmp(r->p,magic,4)!=0)return false;r->n-=4;{Reader tail={r->p+r->n,4,0};if(!get_u32(&tail,&stored))return false;}if(hash32(r->p,r->n)!=stored)return false;r->at=4;if(!get_u32(r,&ver)||ver!=FILE_VERSION)return false;return true;}
static uim_Result read_file(const char*path,unsigned char**out,size_t*out_n){FILE*f;int64_t n;unsigned char*p;if(!path||!path[0]||!out||!out_n)return uim_ERR_ARGUMENT;*out=NULL;*out_n=0;f=uim_fopen_utf8(path,"rb");if(!f)return uim_ERR_NOT_FOUND;if(fseek(f,0,SEEK_END)!=0||(n=(int64_t)ftell(f))<0||n>(int64_t)FILE_LIMIT||fseek(f,0,SEEK_SET)!=0){fclose(f);return uim_ERR_RANGE;}if(n==0){fclose(f);return uim_ERR_RANGE;}p=(unsigned char*)malloc((size_t)n);if(!p){fclose(f);return uim_ERR_NOMEM;}if((size_t)n&&fread(p,1,(size_t)n,f)!=(size_t)n){free(p);fclose(f);return uim_ERR_STATE;}if(fclose(f)!=0){free(p);return uim_ERR_STATE;}*out=p;*out_n=(size_t)n;return uim_OK;}
static uim_Result write_file(const char*path,const unsigned char*p,size_t n){
    FILE*f;char*tmp;size_t path_n;bool ok=true;
    if(!path||!path[0]||!p)return uim_ERR_ARGUMENT;
    if(n>FILE_LIMIT)return uim_ERR_RANGE;
    path_n=strlen(path);if(path_n>SIZE_MAX-5u)return uim_ERR_RANGE;
    tmp=(char*)malloc(path_n+5u);if(!tmp)return uim_ERR_NOMEM;
    memcpy(tmp,path,path_n);memcpy(tmp+path_n,".tmp",5u);
    f=uim_fopen_utf8(tmp,"wb");if(!f){free(tmp);return uim_ERR_STATE;}
    if(n&&fwrite(p,1,n,f)!=n)ok=false;
    if(fflush(f)!=0)ok=false;
    if(fclose(f)!=0)ok=false;
    if(ok){
        ok=uim_move_utf8(tmp,path);
    }
    if(!ok)uim_remove_utf8(tmp);
    free(tmp);return ok?uim_OK:uim_ERR_STATE;
}
static bool rect_valid(uim_Rect r){return isfinite(r.x)&&isfinite(r.y)&&isfinite(r.width)&&isfinite(r.height)&&r.width>=0&&r.height>=0;}

uim_Result uim_layout_save_file(const UIM_ComponentManager*r,const char*path,uim_PageId page){
    Writer w={0};const char*page_key=NULL;size_t count=0,i;uim_Result rc;uim_NodeInfo n,p;
    if(!r||!path||!page)return uim_ERR_ARGUMENT;
    if((rc=uim_page_get_key(r,page,&page_key))!=uim_OK)return rc;
    if((rc=uim_page_get_node_count(r,page,&count))!=uim_OK)return rc;
    if(count>100000u||count>UINT32_MAX)return uim_ERR_RANGE;
    if(!put_bytes(&w,"UIRL",4)||!put_u32(&w,FILE_VERSION)||!put_string(&w,page_key)||!put_u32(&w,(uint32_t)count)){free(w.p);return uim_ERR_NOMEM;}
    for(i=0;i<count;i++){
        const char*parent_key="";
        if(uim_page_get_node_at(r,page,i,&n)!=uim_OK||!n.stable_key||!n.stable_key[0]){free(w.p);return uim_ERR_UNSUPPORTED;}
        if(n.parent_id){if(uim_node_get(r,n.parent_id,&p)!=uim_OK||!p.stable_key){free(w.p);return uim_ERR_STATE;}parent_key=p.stable_key;}
        if(!put_string(&w,n.stable_key)||!put_string(&w,parent_key)||!put_f32(&w,n.local_rect.x)||!put_f32(&w,n.local_rect.y)||!put_f32(&w,n.local_rect.width)||!put_f32(&w,n.local_rect.height)||!put_u8(&w,n.visible?1:0)||!put_u8(&w,n.enabled?1:0)||!put_u32(&w,(uint32_t)n.z_order)||!put_u32(&w,n.child_order)){free(w.p);return uim_ERR_NOMEM;}
    }
    if(!finish_file(&w)){free(w.p);return uim_ERR_NOMEM;}rc=write_file(path,w.p,w.n);free(w.p);return rc;
}

uim_Result uim_layout_load_file(UIM_ComponentManager*r,const char*path,uim_PageId page){
    unsigned char*bytes=NULL;size_t bytes_n=0,i,j,count;Reader rd;char*page_key=NULL;const char*expected=NULL;LayoutRecord*recs=NULL;uim_Result rc;uint32_t n32,z,order;uint8_t vis,en;
    if(!r||!path||!page)return uim_ERR_ARGUMENT;
    rc=read_file(path,&bytes,&bytes_n);if(rc!=uim_OK)return rc;rd=(Reader){bytes,bytes_n,0};
    if(!validate_file(&rd,"UIRL")||!get_string(&rd,&page_key)||!get_u32(&rd,&n32)||n32>100000u){rc=uim_ERR_RANGE;goto done;}
    if(uim_page_get_key(r,page,&expected)!=uim_OK||strcmp(expected,page_key)!=0){rc=uim_ERR_NOT_FOUND;goto done;}
    count=n32;{size_t current=0;if(uim_page_get_node_count(r,page,&current)!=uim_OK||current!=count){rc=uim_ERR_STALE;goto done;}}
    recs=count?(LayoutRecord*)calloc(count,sizeof(*recs)):NULL;if(count&&!recs){rc=uim_ERR_NOMEM;goto done;}
    for(i=0;i<count;i++){
        if(!get_string(&rd,&recs[i].key)||!get_string(&rd,&recs[i].parent)||!get_f32(&rd,&recs[i].rect.x)||!get_f32(&rd,&recs[i].rect.y)||!get_f32(&rd,&recs[i].rect.width)||!get_f32(&rd,&recs[i].rect.height)||!get_u8(&rd,&vis)||!get_u8(&rd,&en)||!get_u32(&rd,&z)||!get_u32(&rd,&order)){rc=uim_ERR_RANGE;goto done;}
        recs[i].visible=vis!=0;recs[i].enabled=en!=0;recs[i].z=(int32_t)z;recs[i].order=order;
        if(vis>1||en>1||!rect_valid(recs[i].rect)||!recs[i].key[0]){rc=uim_ERR_RANGE;goto done;}
        for(j=0;j<i;j++)if(strcmp(recs[j].key,recs[i].key)==0){rc=uim_ERR_EXISTS;goto done;}
        rc=uim_node_find_stable_key(r,page,recs[i].key,&recs[i].id);if(rc!=uim_OK)goto done;
    }
    if(rd.at!=rd.n){rc=uim_ERR_RANGE;goto done;}
    /* Validate all parent links and cycles before mutating any node. */
    for(i=0;i<count;i++){
        if(recs[i].parent[0]){bool found=false;for(j=0;j<count;j++)if(strcmp(recs[j].key,recs[i].parent)==0){found=true;break;}if(!found||strcmp(recs[i].key,recs[i].parent)==0){rc=uim_ERR_STATE;goto done;}}
        {size_t steps=0,k=i;while(recs[k].parent[0]){size_t t;for(t=0;t<count;t++)if(strcmp(recs[t].key,recs[k].parent)==0)break;if(t==count){rc=uim_ERR_STATE;goto done;}k=t;if(++steps>count){rc=uim_ERR_STATE;goto done;}}}
    }
    /* Detach first so a valid target tree can be installed regardless of the
     * old parent topology (no transient cycle during reparenting). */
    for(i=0;i<count;i++){rc=uim_node_set_parent(r,recs[i].id,0);if(rc!=uim_OK)goto done;}
    for(i=0;i<count;i++){
        uim_NodeId parent_id=0;
        if(recs[i].parent[0]){for(j=0;j<count;j++)if(strcmp(recs[j].key,recs[i].parent)==0){parent_id=recs[j].id;break;}}
        rc=uim_node_set_parent(r,recs[i].id,parent_id);if(rc!=uim_OK)goto done;
        if((rc=uim_node_set_rect(r,recs[i].id,recs[i].rect))!=uim_OK)goto done;
        if((rc=uim_node_set_visible(r,recs[i].id,recs[i].visible))!=uim_OK)goto done;
        if((rc=uim_node_set_enabled(r,recs[i].id,recs[i].enabled))!=uim_OK)goto done;
        if((rc=uim_node_set_z_order(r,recs[i].id,recs[i].z))!=uim_OK)goto done;
        if((rc=uim_node_set_child_order(r,recs[i].id,recs[i].order))!=uim_OK)goto done;
    }
    rc=uim_OK;
done:
    if(recs){for(i=0;i<count;i++){free(recs[i].key);free(recs[i].parent);}free(recs);}free(page_key);free(bytes);return rc;
}

static bool put_value(Writer*w,uim_Value v){if(!put_u8(w,(uint8_t)v.type))return false;switch(v.type){case uim_VALUE_BOOL:return put_u8(w,v.as.boolean?1:0);case uim_VALUE_INT64:return put_u64(w,(uint64_t)v.as.integer);case uim_VALUE_FLOAT64:return put_f64(w,v.as.real);case uim_VALUE_STRING:return v.as.string.size<=UINT32_MAX&&put_u32(w,(uint32_t)v.as.string.size)&&put_bytes(w,v.as.string.data,v.as.string.size);default:return false;}}
static bool get_value(Reader*r,uim_Value*out){uint8_t t,b;uint32_t n;uint64_t u;double d;char*s;switch((get_u8(r,&t)?t:255)){case uim_VALUE_BOOL:if(!get_u8(r,&b)||b>1)return false;*out=uim_value_bool(b!=0);return true;case uim_VALUE_INT64:if(!get_u64(r,&u))return false;*out=uim_value_int((int64_t)u);return true;case uim_VALUE_FLOAT64:if(!get_f64(r,&d))return false;*out=uim_value_float(d);return true;case uim_VALUE_STRING:if(!get_u32(r,&n)||n>FILE_LIMIT||n>r->n-r->at)return false;s=(char*)malloc((size_t)n+1);if(!s)return false;if(n)memcpy(s,r->p+r->at,n);s[n]='\0';r->at+=n;out->type=uim_VALUE_STRING;out->as.string.data=s;out->as.string.size=n;return true;default:return false;}}
uim_Result uim_preset_save_file(const UIM_ComponentManager*r,const char*path){Writer w={0};size_t count=0,i;uim_Result rc;uim_PropertyId id;uim_SettingState st;const char*key;if(!r||!path)return uim_ERR_ARGUMENT;if((rc=uim_setting_count(r,&count))!=uim_OK)return rc;if(count>100000u||count>UINT32_MAX)return uim_ERR_RANGE;if(!put_bytes(&w,"UIRP",4)||!put_u32(&w,FILE_VERSION)||!put_u32(&w,(uint32_t)count)){free(w.p);return uim_ERR_NOMEM;}for(i=0;i<count;i++){if((rc=uim_setting_get_at(r,i,&id,&st))!=uim_OK||(rc=uim_property_get_stable_key(r,id,&key))!=uim_OK){free(w.p);return rc;}if(!put_string(&w,key)||!put_value(&w,st.pending)){free(w.p);return uim_ERR_NOMEM;}}if(!finish_file(&w)){free(w.p);return uim_ERR_NOMEM;}rc=write_file(path,w.p,w.n);free(w.p);return rc;}
uim_Result uim_preset_load_file(UIM_ComponentManager*r,const char*path){unsigned char*bytes=NULL;size_t n=0,i,j;Reader rd;uint32_t count;PresetRecord*recs=NULL;uim_PropertyId*ids=NULL;uim_Value*values=NULL;uim_Result rc;if(!r||!path)return uim_ERR_ARGUMENT;rc=read_file(path,&bytes,&n);if(rc!=uim_OK)return rc;rd=(Reader){bytes,n,0};if(!validate_file(&rd,"UIRP")||!get_u32(&rd,&count)||count>100000u){rc=uim_ERR_RANGE;goto done;}recs=count?(PresetRecord*)calloc(count,sizeof(*recs)):NULL;ids=count?(uim_PropertyId*)calloc(count,sizeof(*ids)):NULL;values=count?(uim_Value*)calloc(count,sizeof(*values)):NULL;if(count&&(!recs||!ids||!values)){rc=uim_ERR_NOMEM;goto done;}for(i=0;i<count;i++){if(!get_string(&rd,&recs[i].key)||!get_value(&rd,&recs[i].value)){rc=uim_ERR_RANGE;goto done;}rc=uim_property_find_stable_key(r,recs[i].key,&recs[i].id);if(rc!=uim_OK)goto done;for(j=0;j<i;j++)if(recs[j].id==recs[i].id){rc=uim_ERR_EXISTS;goto done;}ids[i]=recs[i].id;values[i]=recs[i].value;}
if(rd.at!=rd.n){rc=uim_ERR_RANGE;goto done;}rc=uim_setting_set_pending_batch(r,ids,values,count);
done:if(recs){for(i=0;i<count;i++){free(recs[i].key);if(recs[i].value.type==uim_VALUE_STRING)free((void*)recs[i].value.as.string.data);}free(recs);}free(ids);free(values);free(bytes);return rc;}
