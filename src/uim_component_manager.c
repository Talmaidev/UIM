#include "uim_component_manager.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { uim_PageId id; char *key; } Page;
typedef struct { uim_NodeId id; uim_NodeDesc d; char *stable_key; void *config; uim_BindingId binding; } Node;
typedef struct { uim_PropertyId id; char *stable_key; uim_Value value; } Property;
typedef struct { uim_BindingId id; uim_BindingDesc d; } Binding;
typedef struct { uim_PropertyId id; uim_Value initial,committed,pending,snapshot; uint64_t revision; bool snapshot_valid; } Setting;
typedef struct { char *name; uim_PageId page; uim_LayoutNode *nodes; size_t count; } Layout;
typedef struct { char *name; uim_PresetEntry *entries; size_t count; } Preset;
typedef struct { uim_PropertyId property_id; uim_Value value; } ComponentProperty;
typedef struct {
    uim_ComponentInstanceInfo info;
    ComponentProperty *properties;
    size_t property_count, property_cap;
} ComponentInstance;
struct UIM_ComponentManager {
    Page *pages; size_t page_count,page_cap;
    Node *nodes; size_t node_count,node_cap;
    Property *props; size_t prop_count,prop_cap;
    Binding *bindings; size_t binding_count,binding_cap;
    Setting *settings; size_t setting_count,setting_cap;
    Layout *layouts; size_t layout_count,layout_cap;
    Preset *presets; size_t preset_count,preset_cap;
    uim_Event *events; size_t event_count,event_cap;
    ComponentInstance *component_instances; size_t component_instance_count,component_instance_cap;
    uint64_t next_page,next_node,next_binding,next_serial,next_event,next_component_instance,active_serial;
    bool apply_pending;
};
static bool grow(void **p,size_t *cap,size_t need,size_t elem){size_t n;void*q;if(need<=*cap)return true;n=*cap?*cap:8;while(n<need){if(n>SIZE_MAX/2)return false;n*=2;}if(elem&&n>SIZE_MAX/elem)return false;q=realloc(*p,n*elem);if(!q)return false;*p=q;*cap=n;return true;}
static char *dup_n(const char*s,size_t n){char*p;if(!s&&n)return NULL;p=(char*)malloc(n+1);if(!p)return NULL;if(n)memcpy(p,s,n);p[n]=0;return p;}
static bool value_valid(uim_Value v){if(v.type<uim_VALUE_NONE||v.type>uim_VALUE_STRING)return false;if(v.type==uim_VALUE_FLOAT64&&!isfinite(v.as.real))return false;if(v.type==uim_VALUE_STRING&&(!v.as.string.data&&v.as.string.size))return false;return true;}
static uim_Result value_copy(uim_Value *dst,uim_Value src){if(!dst||!value_valid(src))return uim_ERR_ARGUMENT;memset(dst,0,sizeof(*dst));if(src.type==uim_VALUE_STRING){char*p=dup_n(src.as.string.data,src.as.string.size);if(!p)return uim_ERR_NOMEM;*dst=src;dst->as.string.data=p;}else *dst=src;return uim_OK;}
static void value_free(uim_Value*v){if(!v)return;if(v->type==uim_VALUE_STRING)free((void*)v->as.string.data);memset(v,0,sizeof(*v));}
uim_Value uim_value_bool(bool v){uim_Value x={0};x.type=uim_VALUE_BOOL;x.as.boolean=v;return x;}
uim_Value uim_value_int(int64_t v){uim_Value x={0};x.type=uim_VALUE_INT64;x.as.integer=v;return x;}
uim_Value uim_value_float(double v){uim_Value x={0};x.type=uim_VALUE_FLOAT64;x.as.real=v;return x;}
uim_Value uim_value_string(const char*v,size_t n){uim_Value x={0};x.type=uim_VALUE_STRING;x.as.string.data=v;x.as.string.size=n;return x;}
bool uim_value_equal(uim_Value a,uim_Value b){if(a.type!=b.type)return false;switch(a.type){case uim_VALUE_NONE:return true;case uim_VALUE_BOOL:return a.as.boolean==b.as.boolean;case uim_VALUE_INT64:return a.as.integer==b.as.integer;case uim_VALUE_FLOAT64:return isfinite(a.as.real)&&isfinite(b.as.real)&&a.as.real==b.as.real;case uim_VALUE_STRING:return a.as.string.size==b.as.string.size&&(!a.as.string.size||(a.as.string.data&&b.as.string.data&&memcmp(a.as.string.data,b.as.string.data,a.as.string.size)==0));default:return false;}}
static Page *page_find(UIM_ComponentManager*r,uim_PageId id){size_t i;for(i=0;i<r->page_count;i++)if(r->pages[i].id==id)return &r->pages[i];return NULL;}
static Node *node_find(UIM_ComponentManager*r,uim_NodeId id){size_t i;for(i=0;i<r->node_count;i++)if(r->nodes[i].id==id)return &r->nodes[i];return NULL;}
static const Node *node_find_c(const UIM_ComponentManager*r,uim_NodeId id){size_t i;for(i=0;i<r->node_count;i++)if(r->nodes[i].id==id)return &r->nodes[i];return NULL;}
static Property *prop_find(UIM_ComponentManager*r,uim_PropertyId id){size_t i;for(i=0;i<r->prop_count;i++)if(r->props[i].id==id)return &r->props[i];return NULL;}
static const Property *prop_find_c(const UIM_ComponentManager*r,uim_PropertyId id){size_t i;for(i=0;i<r->prop_count;i++)if(r->props[i].id==id)return &r->props[i];return NULL;}
static Setting *setting_find(UIM_ComponentManager*r,uim_PropertyId id){size_t i;for(i=0;i<r->setting_count;i++)if(r->settings[i].id==id)return &r->settings[i];return NULL;}
static const Setting *setting_find_c(const UIM_ComponentManager*r,uim_PropertyId id){size_t i;for(i=0;i<r->setting_count;i++)if(r->settings[i].id==id)return &r->settings[i];return NULL;}
static Binding *binding_find(UIM_ComponentManager*r,uim_BindingId id){size_t i;for(i=0;i<r->binding_count;i++)if(r->bindings[i].id==id)return &r->bindings[i];return NULL;}
static void node_info(const Node*n,uim_NodeInfo*out);
static bool rect_ok(uim_Rect r){return isfinite(r.x)&&isfinite(r.y)&&isfinite(r.width)&&isfinite(r.height)&&r.width>=0&&r.height>=0;}
static bool transform3_ok(const uim_Transform3*t){size_t i;if(!t)return false;for(i=0;i<3;i++)if(!isfinite(t->position[i])||!isfinite(t->rotation[i])||!isfinite(t->scale[i]))return false;return true;}
static bool layout_constraints_ok(uim_LayoutConstraints c){return !c.enabled||(isfinite(c.anchor_min_x)&&isfinite(c.anchor_min_y)&&isfinite(c.anchor_max_x)&&isfinite(c.anchor_max_y)&&c.anchor_min_x>=0.0f&&c.anchor_min_x<=1.0f&&c.anchor_min_y>=0.0f&&c.anchor_min_y<=1.0f&&c.anchor_max_x>=0.0f&&c.anchor_max_x<=1.0f&&c.anchor_max_y>=0.0f&&c.anchor_max_y<=1.0f&&c.anchor_min_x<=c.anchor_max_x&&c.anchor_min_y<=c.anchor_max_y&&isfinite(c.offset_left)&&isfinite(c.offset_top)&&isfinite(c.offset_right)&&isfinite(c.offset_bottom));}
static void remove_binding_for_node(UIM_ComponentManager*r,Node*n){size_t i;if(!n->binding)return;for(i=0;i<r->binding_count;i++)if(r->bindings[i].id==n->binding){memmove(&r->bindings[i],&r->bindings[i+1],(r->binding_count-i-1)*sizeof(Binding));r->binding_count--;break;}n->binding=0;}
static void component_instance_remove_at(UIM_ComponentManager*r,size_t i){ComponentInstance*c=&r->component_instances[i];size_t j;for(j=0;j<c->property_count;j++)value_free(&c->properties[j].value);free(c->properties);memmove(c,c+1,(r->component_instance_count-i-1)*sizeof(*c));r->component_instance_count--;}
static void component_instances_remove_for_node(UIM_ComponentManager*r,uim_NodeId node_id){size_t i=0;while(i<r->component_instance_count){if(r->component_instances[i].info.node_id==node_id)component_instance_remove_at(r,i);else i++;}}
static void node_remove_index(UIM_ComponentManager*r,size_t i){Node*n=&r->nodes[i];component_instances_remove_for_node(r,n->id);remove_binding_for_node(r,n);free(n->stable_key);free(n->config);memmove(n,n+1,(r->node_count-i-1)*sizeof(Node));r->node_count--;}
static void node_destroy_recursive(UIM_ComponentManager*r,uim_NodeId id){size_t i=0;while(i<r->node_count){if(r->nodes[i].d.parent_id==id){uim_NodeId child=r->nodes[i].id;node_destroy_recursive(r,child);i=0;}else i++;}for(i=0;i<r->node_count;i++)if(r->nodes[i].id==id){node_remove_index(r,i);return;}}
static void layout_nodes_free(uim_LayoutNode*n,size_t count){size_t i;if(!n)return;for(i=0;i<count;i++){free((void*)n[i].stable_key);free((void*)n[i].parent_stable_key);}free(n);}
static void layout_free(Layout*l){free(l->name);layout_nodes_free(l->nodes,l->count);memset(l,0,sizeof(*l));}
static void preset_free(Preset*p){size_t i;free(p->name);for(i=0;i<p->count;i++)value_free(&p->entries[i].value);free(p->entries);memset(p,0,sizeof(*p));}
uim_Result uim_component_manager_create(const UIM_ComponentManagerConfig*c,UIM_ComponentManager**out){UIM_ComponentManager*r;if(!out)return uim_ERR_ARGUMENT;*out=NULL;r=(UIM_ComponentManager*)calloc(1,sizeof(*r));if(!r)return uim_ERR_NOMEM;r->next_page=r->next_node=r->next_binding=r->next_serial=r->next_event=r->next_component_instance=1;if(c){if(c->initial_page_capacity&&!grow((void**)&r->pages,&r->page_cap,c->initial_page_capacity,sizeof(Page)))goto oom;if(c->initial_node_capacity&&!grow((void**)&r->nodes,&r->node_cap,c->initial_node_capacity,sizeof(Node)))goto oom;}*out=r;return uim_OK;oom:uim_component_manager_destroy(r);return uim_ERR_NOMEM;}
void uim_component_manager_destroy(UIM_ComponentManager*r){size_t i;if(!r)return;for(i=0;i<r->page_count;i++)free(r->pages[i].key);for(i=0;i<r->node_count;i++){free(r->nodes[i].stable_key);free(r->nodes[i].config);}for(i=0;i<r->prop_count;i++){free(r->props[i].stable_key);value_free(&r->props[i].value);}for(i=0;i<r->setting_count;i++){value_free(&r->settings[i].initial);value_free(&r->settings[i].committed);value_free(&r->settings[i].pending);value_free(&r->settings[i].snapshot);}for(i=0;i<r->layout_count;i++)layout_free(&r->layouts[i]);for(i=0;i<r->preset_count;i++)preset_free(&r->presets[i]);for(i=0;i<r->event_count;i++)if(r->events[i].value.type==uim_VALUE_STRING)value_free(&r->events[i].value);for(i=0;i<r->component_instance_count;i++){size_t j;for(j=0;j<r->component_instances[i].property_count;j++)value_free(&r->component_instances[i].properties[j].value);free(r->component_instances[i].properties);}free(r->component_instances);free(r->pages);free(r->nodes);free(r->props);free(r->bindings);free(r->settings);free(r->layouts);free(r->presets);free(r->events);free(r);}
uim_Result uim_page_create(UIM_ComponentManager*r,const char*k,uim_PageId*out){size_t i;Page*p;if(!r||!k||!k[0]||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->page_count;i++)if(strcmp(r->pages[i].key,k)==0)return uim_ERR_EXISTS;if(r->next_page==0||r->next_page==UINT64_MAX)return uim_ERR_RANGE;if(!grow((void**)&r->pages,&r->page_cap,r->page_count+1,sizeof(Page)))return uim_ERR_NOMEM;p=&r->pages[r->page_count++];p->key=dup_n(k,strlen(k));if(!p->key){r->page_count--;return uim_ERR_NOMEM;}p->id=r->next_page++;*out=p->id;return uim_OK;}
uim_Result uim_page_find(const UIM_ComponentManager*r,const char*k,uim_PageId*out){size_t i;if(!r||!k||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->page_count;i++)if(strcmp(r->pages[i].key,k)==0){*out=r->pages[i].id;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_page_get_key(const UIM_ComponentManager*r,uim_PageId id,const char**out){size_t i;if(!r||!id||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->page_count;i++)if(r->pages[i].id==id){*out=r->pages[i].key;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_page_count(const UIM_ComponentManager*r,size_t*out){if(!r||!out)return uim_ERR_ARGUMENT;*out=r->page_count;return uim_OK;}
uim_Result uim_page_get_at(const UIM_ComponentManager*r,size_t index,uim_PageId*out){if(!r||!out)return uim_ERR_ARGUMENT;if(index>=r->page_count)return uim_ERR_NOT_FOUND;*out=r->pages[index].id;return uim_OK;}
uim_Result uim_page_get_node_count(const UIM_ComponentManager*r,uim_PageId page,size_t*out){size_t i,n=0;if(!r||!page||!out)return uim_ERR_ARGUMENT;if(!page_find((UIM_ComponentManager*)r,page))return uim_ERR_NOT_FOUND;for(i=0;i<r->node_count;i++)if(r->nodes[i].d.page_id==page)n++;*out=n;return uim_OK;}
uim_Result uim_page_get_node_at(const UIM_ComponentManager*r,uim_PageId page,size_t index,uim_NodeInfo*out){size_t i,n=0;if(!r||!page||!out)return uim_ERR_ARGUMENT;if(!page_find((UIM_ComponentManager*)r,page))return uim_ERR_NOT_FOUND;for(i=0;i<r->node_count;i++)if(r->nodes[i].d.page_id==page){if(n++==index){node_info(&r->nodes[i],out);return uim_OK;}}return uim_ERR_NOT_FOUND;}
uim_Result uim_node_find_stable_key(const UIM_ComponentManager*r,uim_PageId page,const char*key,uim_NodeId*out){size_t i;if(!r||!page||!key||!key[0]||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->node_count;i++)if(r->nodes[i].d.page_id==page&&r->nodes[i].stable_key&&strcmp(r->nodes[i].stable_key,key)==0){*out=r->nodes[i].id;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_page_destroy(UIM_ComponentManager*r,uim_PageId id){size_t i;if(!r||!id)return uim_ERR_ARGUMENT;if(!page_find(r,id))return uim_ERR_NOT_FOUND;i=0;while(i<r->node_count){if(r->nodes[i].d.page_id==id){node_destroy_recursive(r,r->nodes[i].id);i=0;}else i++;}for(i=0;i<r->page_count;i++)if(r->pages[i].id==id){free(r->pages[i].key);memmove(&r->pages[i],&r->pages[i+1],(r->page_count-i-1)*sizeof(Page));r->page_count--;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_node_create(UIM_ComponentManager*r,const uim_NodeDesc*d,uim_NodeId*out){Node*n;void*cfg=NULL;char*key=NULL;size_t i;if(!r||!d||!out||!page_find(r,d->page_id)||(d->space!=uim_SPACE_2D&&d->space!=uim_SPACE_3D)||(d->space==uim_SPACE_3D&&!transform3_ok(&d->transform))||!rect_ok(d->local_rect)||!layout_constraints_ok(d->layout)||(!d->component_config&&d->component_config_size))return uim_ERR_ARGUMENT;if(r->next_node==0||r->next_node==UINT64_MAX)return uim_ERR_RANGE;if(d->stable_key&&d->stable_key[0]){for(i=0;i<r->node_count;i++)if(r->nodes[i].d.page_id==d->page_id&&r->nodes[i].stable_key&&strcmp(r->nodes[i].stable_key,d->stable_key)==0)return uim_ERR_EXISTS;key=dup_n(d->stable_key,strlen(d->stable_key));if(!key)return uim_ERR_NOMEM;}if(d->parent_id){Node*p=node_find(r,d->parent_id);if(!p||p->d.page_id!=d->page_id){free(key);return uim_ERR_NOT_FOUND;}}if(d->component_config_size){cfg=malloc(d->component_config_size);if(!cfg){free(key);return uim_ERR_NOMEM;}memcpy(cfg,d->component_config,d->component_config_size);}if(!grow((void**)&r->nodes,&r->node_cap,r->node_count+1,sizeof(Node))){free(cfg);free(key);return uim_ERR_NOMEM;}n=&r->nodes[r->node_count++];memset(n,0,sizeof(*n));n->id=r->next_node++;n->d=*d;n->d.stable_key=NULL;n->d.component_config=NULL;n->stable_key=key;n->config=cfg;*out=n->id;return uim_OK;}
uim_Result uim_node_set_stable_key(UIM_ComponentManager*r,uim_NodeId id,const char*key){Node*n;char*copy;size_t i;if(!r||!id||!key||!key[0])return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;for(i=0;i<r->node_count;i++)if(i!=(size_t)(n-r->nodes)&&r->nodes[i].d.page_id==n->d.page_id&&r->nodes[i].stable_key&&strcmp(r->nodes[i].stable_key,key)==0)return uim_ERR_EXISTS;if(n->stable_key&&strcmp(n->stable_key,key)==0)return uim_OK;copy=dup_n(key,strlen(key));if(!copy)return uim_ERR_NOMEM;free(n->stable_key);n->stable_key=copy;return uim_OK;}
uim_Result uim_node_destroy(UIM_ComponentManager*r,uim_NodeId id){if(!r||!id)return uim_ERR_ARGUMENT;if(!node_find(r,id))return uim_ERR_NOT_FOUND;node_destroy_recursive(r,id);return uim_OK;}
static void node_info(const Node*n,uim_NodeInfo*out){out->stable_key=n->stable_key;out->id=n->id;out->page_id=n->d.page_id;out->component_id=n->d.component_id;out->parent_id=n->d.parent_id;out->local_rect=n->d.local_rect;out->layout=n->d.layout;out->visible=n->d.visible;out->enabled=n->d.enabled;out->z_order=n->d.z_order;out->action_id=n->d.action_id;out->child_order=n->d.child_order;out->binding_id=n->binding;out->component_config=n->config;out->component_config_size=n->d.component_config_size;out->space=n->d.space;out->transform=n->d.transform;}
uim_Result uim_node_get(const UIM_ComponentManager*r,uim_NodeId id,uim_NodeInfo*out){const Node*n;if(!r||!id||!out)return uim_ERR_ARGUMENT;n=node_find_c(r,id);if(!n)return uim_ERR_NOT_FOUND;node_info(n,out);return uim_OK;}
uim_Result uim_node_get_child(const UIM_ComponentManager*r,uim_NodeId parent,size_t index,uim_NodeInfo*out){size_t i,j,rank,best=SIZE_MAX;if(!r||!parent||!out)return uim_ERR_ARGUMENT;if(!node_find_c(r,parent))return uim_ERR_NOT_FOUND;for(i=0;i<r->node_count;i++){if(r->nodes[i].d.parent_id!=parent)continue;rank=0;for(j=0;j<r->node_count;j++)if(r->nodes[j].d.parent_id==parent&&(r->nodes[j].d.child_order<r->nodes[i].d.child_order||(r->nodes[j].d.child_order==r->nodes[i].d.child_order&&r->nodes[j].id<r->nodes[i].id)))rank++;if(rank==index){best=i;break;}}if(best==SIZE_MAX)return uim_ERR_NOT_FOUND;node_info(&r->nodes[best],out);return uim_OK;}
uim_Result uim_node_set_parent(UIM_ComponentManager*r,uim_NodeId id,uim_NodeId parent){Node*n,*p;uim_NodeId cur;if(!r||!id)return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;if(parent){p=node_find(r,parent);if(!p||p->d.page_id!=n->d.page_id)return uim_ERR_NOT_FOUND;cur=parent;while(cur){if(cur==id)return uim_ERR_STATE;p=node_find(r,cur);cur=p?p->d.parent_id:0;}}n->d.parent_id=parent;return uim_OK;}
uim_Result uim_node_set_rect(UIM_ComponentManager*r,uim_NodeId id,uim_Rect v){Node*n;if(!r||!id||!rect_ok(v))return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;if(n->d.space!=uim_SPACE_2D)return uim_ERR_TYPE;n->d.local_rect=v;return uim_OK;}
uim_Result uim_node_set_space(UIM_ComponentManager*r,uim_NodeId id,uim_Space space){Node*n;if(!r||!id||(space!=uim_SPACE_2D&&space!=uim_SPACE_3D))return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;n->d.space=space;return uim_OK;}
uim_Result uim_node_set_transform3(UIM_ComponentManager*r,uim_NodeId id,const uim_Transform3*t){Node*n;if(!r||!id||!transform3_ok(t))return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;if(n->d.space!=uim_SPACE_3D)return uim_ERR_TYPE;n->d.transform=*t;return uim_OK;}
uim_Result uim_node_get_transform3(const UIM_ComponentManager*r,uim_NodeId id,uim_Transform3*out){const Node*n;if(!r||!id||!out)return uim_ERR_ARGUMENT;n=node_find_c(r,id);if(!n)return uim_ERR_NOT_FOUND;if(n->d.space!=uim_SPACE_3D)return uim_ERR_TYPE;*out=n->d.transform;return uim_OK;}
uim_Result uim_node_set_layout_constraints(UIM_ComponentManager*r,uim_NodeId id,uim_LayoutConstraints v){Node*n;if(!r||!id||!layout_constraints_ok(v))return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;n->d.layout=v;return uim_OK;}
uim_Result uim_node_get_layout_constraints(const UIM_ComponentManager*r,uim_NodeId id,uim_LayoutConstraints*out){const Node*n;if(!r||!id||!out)return uim_ERR_ARGUMENT;n=node_find_c(r,id);if(!n)return uim_ERR_NOT_FOUND;*out=n->d.layout;return uim_OK;}
uim_Result uim_page_resolve_layout(UIM_ComponentManager*r,uim_PageId page,float vw,float vh){
    typedef struct LayoutWork { uim_NodeInfo info; uim_Rect resolved; uint32_t depth; } LayoutWork;
    LayoutWork *items=NULL;size_t count=0,i,j;uim_Result rc;
    if(!r||!page||!isfinite(vw)||!isfinite(vh)||vw<=0.0f||vh<=0.0f)return uim_ERR_ARGUMENT;
    if((rc=uim_page_get_node_count(r,page,&count))!=uim_OK)return rc;
    if(!count)return uim_OK;
    if(count>SIZE_MAX/sizeof(*items))return uim_ERR_RANGE;
    items=(LayoutWork*)calloc(count,sizeof(*items));if(!items)return uim_ERR_NOMEM;
    for(i=0;i<count;i++){
        uim_NodeInfo n;uim_NodeId parent;size_t guard=0;
        if((rc=uim_page_get_node_at(r,page,i,&n))!=uim_OK)goto done;
        items[i].info=n;items[i].resolved=n.local_rect;parent=n.parent_id;
        while(parent){uim_NodeInfo p;if(++guard>count){rc=uim_ERR_STATE;goto done;}if(uim_node_get(r,parent,&p)!=uim_OK){rc=uim_ERR_STATE;goto done;}if(p.page_id!=page){rc=uim_ERR_STATE;goto done;}parent=p.parent_id;items[i].depth++;}
    }
    /* Parent-before-child order, independent of creation order. */
    for(i=0;i<count;i++)for(j=i+1;j<count;j++)if(items[j].depth<items[i].depth){LayoutWork t=items[i];items[i]=items[j];items[j]=t;}
    for(i=0;i<count;i++){
        uim_LayoutConstraints c=items[i].info.layout;float pw=vw,ph=vh;
        if(!c.enabled)continue;
        if(items[i].info.parent_id){bool found=false;for(j=0;j<i;j++)if(items[j].info.id==items[i].info.parent_id){pw=items[j].resolved.width;ph=items[j].resolved.height;found=true;break;}if(!found){rc=uim_ERR_STATE;goto done;}}
        {float left=pw*c.anchor_min_x+c.offset_left;float top=ph*c.anchor_min_y+c.offset_top;float right=pw*c.anchor_max_x-c.offset_right;float bottom=ph*c.anchor_max_y-c.offset_bottom;
         if(!isfinite(left)||!isfinite(top)||!isfinite(right)||!isfinite(bottom)||right<left||bottom<top){rc=uim_ERR_RANGE;goto done;}
         items[i].resolved=(uim_Rect){left,top,right-left,bottom-top};}
    }
    /* All target rectangles are valid before the first mutation. */
    for(i=0;i<count;i++){if(items[i].info.space==uim_SPACE_3D)continue;rc=uim_node_set_rect(r,items[i].info.id,items[i].resolved);if(rc!=uim_OK){for(j=0;j<i;j++)if(items[j].info.space==uim_SPACE_2D)(void)uim_node_set_rect(r,items[j].info.id,items[j].info.local_rect);goto done;}}
    rc=uim_OK;
done:free(items);return rc;
}
uim_Result uim_node_set_visible(UIM_ComponentManager*r,uim_NodeId id,bool v){Node*n;if(!r||!id)return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;n->d.visible=v;return uim_OK;}
uim_Result uim_node_set_enabled(UIM_ComponentManager*r,uim_NodeId id,bool v){Node*n;if(!r||!id)return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;n->d.enabled=v;return uim_OK;}
uim_Result uim_node_set_z_order(UIM_ComponentManager*r,uim_NodeId id,int32_t v){Node*n;if(!r||!id)return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;n->d.z_order=v;return uim_OK;}
uim_Result uim_node_set_action_id(UIM_ComponentManager*r,uim_NodeId id,uim_ActionId action){Node*n;if(!r||!id)return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;n->d.action_id=action;return uim_OK;}
uim_Result uim_node_set_child_order(UIM_ComponentManager*r,uim_NodeId id,uint32_t v){Node*n;if(!r||!id)return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;n->d.child_order=v;return uim_OK;}
uim_Result uim_node_set_component_config(UIM_ComponentManager*r,uim_NodeId id,const void*d,size_t z){Node*n;void*p=NULL;if(!r||!id||(!d&&z))return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;if(z){p=malloc(z);if(!p)return uim_ERR_NOMEM;memcpy(p,d,z);}free(n->config);n->config=p;n->d.component_config_size=z;return uim_OK;}
uim_Result uim_property_register_named(UIM_ComponentManager*r,uim_PropertyId id,const char*stable_key,uim_Value v){Property*p;Setting*s;uim_Value a={0},b={0},c={0},d={0};char fallback[32];char*key=NULL;if(!r||!id||!value_valid(v)||v.type==uim_VALUE_NONE)return uim_ERR_ARGUMENT;if(prop_find(r,id))return uim_ERR_EXISTS;if(!stable_key||!stable_key[0]){(void)snprintf(fallback,sizeof(fallback),"id:%u",(unsigned)id);stable_key=fallback;}{size_t q;for(q=0;q<r->prop_count;q++)if(strcmp(r->props[q].stable_key,stable_key)==0)return uim_ERR_EXISTS;}key=dup_n(stable_key,strlen(stable_key));if(!key)return uim_ERR_NOMEM;if(value_copy(&a,v)!=uim_OK||value_copy(&b,v)!=uim_OK||value_copy(&c,v)!=uim_OK||value_copy(&d,v)!=uim_OK){free(key);value_free(&a);value_free(&b);value_free(&c);value_free(&d);return uim_ERR_NOMEM;}if(!grow((void**)&r->props,&r->prop_cap,r->prop_count+1,sizeof(Property))||!grow((void**)&r->settings,&r->setting_cap,r->setting_count+1,sizeof(Setting))){free(key);value_free(&a);value_free(&b);value_free(&c);value_free(&d);return uim_ERR_NOMEM;}p=&r->props[r->prop_count++];p->id=id;p->stable_key=key;p->value=a;s=&r->settings[r->setting_count++];memset(s,0,sizeof(*s));s->id=id;s->initial=b;s->committed=c;s->pending=d;return uim_OK;}
uim_Result uim_property_register(UIM_ComponentManager*r,uim_PropertyId id,uim_Value v){return uim_property_register_named(r,id,NULL,v);}
uim_Result uim_property_get_stable_key(const UIM_ComponentManager*r,uim_PropertyId id,const char**out){size_t i;if(!r||!id||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->prop_count;i++)if(r->props[i].id==id){*out=r->props[i].stable_key;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_property_find_stable_key(const UIM_ComponentManager*r,const char*key,uim_PropertyId*out){size_t i;if(!r||!key||!key[0]||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->prop_count;i++)if(strcmp(r->props[i].stable_key,key)==0){*out=r->props[i].id;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_setting_count(const UIM_ComponentManager*r,size_t*out){if(!r||!out)return uim_ERR_ARGUMENT;*out=r->setting_count;return uim_OK;}
uim_Result uim_setting_get_at(const UIM_ComponentManager*r,size_t index,uim_PropertyId*out_id,uim_SettingState*out){if(!r||!out_id||!out)return uim_ERR_ARGUMENT;if(index>=r->setting_count)return uim_ERR_NOT_FOUND;*out_id=r->settings[index].id;return uim_setting_get_state(r,*out_id,out);}
uim_Result uim_binding_create(UIM_ComponentManager*r,const uim_BindingDesc*d,uim_BindingId*out){
    Node*n;Property*p;Setting*s;Binding*b;uim_Value a={0},c={0};bool sync_changed=false;
    if(!r||!d||!out||!d->node_id||!d->property_id||d->direction<uim_BIND_READ||d->direction>uim_BIND_READ_WRITE||d->initial_sync<uim_SYNC_EXTERNAL_WINS||d->initial_sync>uim_SYNC_INITIAL_ONLY||d->state<uim_BINDING_ACTIVE||d->state>uim_BINDING_BROKEN)return uim_ERR_ARGUMENT;
    n=node_find(r,d->node_id);p=prop_find(r,d->property_id);s=setting_find(r,d->property_id);
    if(!n||!p||!s)return uim_ERR_NOT_FOUND;
    if(p->value.type!=d->value_type)return uim_ERR_TYPE;
    if((d->initial_sync==uim_SYNC_EXTERNAL_WINS&&!(d->direction&uim_BIND_READ))||(d->initial_sync==uim_SYNC_PENDING_WINS&&!(d->direction&uim_BIND_WRITE)))return uim_ERR_ARGUMENT;
    if(n->binding)return uim_ERR_EXISTS;
    if(r->next_binding==0||r->next_binding==UINT64_MAX)return uim_ERR_RANGE;
    if(d->initial_sync==uim_SYNC_EXTERNAL_WINS){
        sync_changed=!uim_value_equal(s->committed,p->value)||!uim_value_equal(s->pending,p->value);
        if(sync_changed&&s->revision==UINT64_MAX)return uim_ERR_RANGE;
        if(value_copy(&a,p->value)!=uim_OK||value_copy(&c,p->value)!=uim_OK){value_free(&a);value_free(&c);return uim_ERR_NOMEM;}
    }else if(d->initial_sync==uim_SYNC_PENDING_WINS){
        if(value_copy(&a,s->pending)!=uim_OK)return uim_ERR_NOMEM;
    }
    if(!grow((void**)&r->bindings,&r->binding_cap,r->binding_count+1,sizeof(Binding))){value_free(&a);value_free(&c);return uim_ERR_NOMEM;}
    b=&r->bindings[r->binding_count++];b->id=r->next_binding++;b->d=*d;n->binding=b->id;*out=b->id;
    if(d->initial_sync==uim_SYNC_EXTERNAL_WINS){value_free(&s->committed);s->committed=a;value_free(&s->pending);s->pending=c;if(sync_changed)s->revision++;}
    else if(d->initial_sync==uim_SYNC_PENDING_WINS){value_free(&p->value);p->value=a;}
    return uim_OK;
}
uim_Result uim_binding_restore(UIM_ComponentManager*r,const uim_BindingDesc*d,uim_BindingId*out){uim_BindingDesc safe;Binding*b;uim_Result rc;if(!r||!d||!out)return uim_ERR_ARGUMENT;if((d->initial_sync==uim_SYNC_EXTERNAL_WINS&&!(d->direction&uim_BIND_READ))||(d->initial_sync==uim_SYNC_PENDING_WINS&&!(d->direction&uim_BIND_WRITE)))return uim_ERR_ARGUMENT;safe=*d;safe.initial_sync=uim_SYNC_INITIAL_ONLY;rc=uim_binding_create(r,&safe,out);if(rc!=uim_OK)return rc;b=binding_find(r,*out);if(!b){(void)uim_binding_remove(r,*out);return uim_ERR_STATE;}b->d.initial_sync=d->initial_sync;return uim_OK;}
uim_Result uim_binding_remove(UIM_ComponentManager*r,uim_BindingId id){size_t i;Binding*b;if(!r||!id)return uim_ERR_ARGUMENT;b=binding_find(r,id);if(!b)return uim_ERR_NOT_FOUND;Node*n=node_find(r,b->d.node_id);if(n)n->binding=0;for(i=0;i<r->binding_count;i++)if(r->bindings[i].id==id){memmove(&r->bindings[i],&r->bindings[i+1],(r->binding_count-i-1)*sizeof(Binding));r->binding_count--;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_binding_set_state(UIM_ComponentManager*r,uim_BindingId id,uim_BindingState state){Binding*b;if(!r||!id||state<uim_BINDING_ACTIVE||state>uim_BINDING_BROKEN)return uim_ERR_ARGUMENT;b=binding_find(r,id);if(!b)return uim_ERR_NOT_FOUND;b->d.state=state;return uim_OK;}
uim_Result uim_binding_get(const UIM_ComponentManager*r,uim_BindingId id,uim_BindingDesc*out){size_t i;if(!r||!id||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->binding_count;i++)if(r->bindings[i].id==id){*out=r->bindings[i].d;return uim_OK;}return uim_ERR_NOT_FOUND;}
static uim_Result set_pending_internal(UIM_ComponentManager*r,uim_PropertyId id,uim_Value v){Setting*s=setting_find(r,id);uim_Value cp={0};if(!s)return uim_ERR_NOT_FOUND;if(v.type!=s->pending.type)return uim_ERR_TYPE;if(uim_value_equal(s->pending,v))return uim_OK;if(s->revision==UINT64_MAX)return uim_ERR_RANGE;if(value_copy(&cp,v)!=uim_OK)return uim_ERR_NOMEM;value_free(&s->pending);s->pending=cp;s->revision++;return uim_OK;}
uim_Result uim_property_update(UIM_ComponentManager*r,uim_PropertyId id,uim_Value v){Property*p;Setting*s;uim_Value cp={0},cc={0},pending_copy={0};bool was_dirty,changed;if(!r||!id||!value_valid(v))return uim_ERR_ARGUMENT;p=prop_find(r,id);s=setting_find(r,id);if(!p||!s)return uim_ERR_NOT_FOUND;if(v.type!=p->value.type)return uim_ERR_TYPE;was_dirty=!uim_value_equal(s->pending,s->committed);changed=!uim_value_equal(p->value,v)||!uim_value_equal(s->committed,v)||(!was_dirty&&!uim_value_equal(s->pending,v));if(!changed)return uim_OK;if(s->revision==UINT64_MAX)return uim_ERR_RANGE;if(value_copy(&cp,v)!=uim_OK||value_copy(&cc,v)!=uim_OK||value_copy(&pending_copy,was_dirty?s->pending:v)!=uim_OK){value_free(&cp);value_free(&cc);value_free(&pending_copy);return uim_ERR_NOMEM;}value_free(&p->value);p->value=cp;value_free(&s->committed);s->committed=cc;value_free(&s->pending);s->pending=pending_copy;s->revision++;return uim_OK;}
uim_Result uim_property_get(const UIM_ComponentManager*r,uim_PropertyId id,uim_Value*out){const Property*p;if(!r||!id||!out)return uim_ERR_ARGUMENT;p=prop_find_c(r,id);if(!p)return uim_ERR_NOT_FOUND;*out=p->value;return uim_OK;}
static uim_Result event_push(UIM_ComponentManager*r,uim_Event e){uim_Event*x;if(r->next_event==0||r->next_event==UINT64_MAX)return uim_ERR_RANGE;if(!grow((void**)&r->events,&r->event_cap,r->event_count+1,sizeof(uim_Event)))return uim_ERR_NOMEM;if(e.value.type==uim_VALUE_STRING){uim_Value cp={0};if(value_copy(&cp,e.value)!=uim_OK)return uim_ERR_NOMEM;e.value=cp;}e.sequence=r->next_event++;x=&r->events[r->event_count++];*x=e;return uim_OK;}
uim_Result uim_user_value_changed(UIM_ComponentManager*r,uim_NodeId id,uim_Value v){Node*n;Binding*b=NULL;Property*p;Setting*s;uim_Value old={0};uim_Event e;size_t i;uint64_t old_revision;uim_Result rc;if(!r||!id||!value_valid(v))return uim_ERR_ARGUMENT;n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;for(i=0;i<r->binding_count;i++)if(r->bindings[i].id==n->binding){b=&r->bindings[i];break;}if(!b||b->d.state!=uim_BINDING_ACTIVE||!(b->d.direction&uim_BIND_WRITE))return uim_ERR_STATE;if(v.type!=b->d.value_type)return uim_ERR_TYPE;p=prop_find(r,b->d.property_id);s=setting_find(r,b->d.property_id);if(!p||!s)return uim_ERR_NOT_FOUND;if(uim_value_equal(s->pending,v))return uim_OK;old_revision=s->revision;if(value_copy(&old,s->pending)!=uim_OK)return uim_ERR_NOMEM;rc=set_pending_internal(r,b->d.property_id,v);if(rc!=uim_OK){value_free(&old);return rc;}e=(uim_Event){0};e.type=uim_EVENT_USER_VALUE_CHANGED;e.node_id=id;e.property_id=b->d.property_id;e.value=v;rc=event_push(r,e);if(rc!=uim_OK){value_free(&s->pending);s->pending=old;s->revision=old_revision;return rc;}value_free(&old);return uim_OK;}
uim_Result uim_user_action(UIM_ComponentManager*r,uim_NodeId id,uim_Value payload){
    Node*n;uim_Event e;uim_NodeId parent;size_t guard=0;uim_Result rc;
    if(!r||!id||!value_valid(payload))return uim_ERR_ARGUMENT;
    n=node_find(r,id);if(!n)return uim_ERR_NOT_FOUND;
    if(n->d.action_id==uim_ACTION_INVALID)return uim_ERR_STATE;
    if(!n->d.visible||!n->d.enabled)return uim_ERR_STATE;
    parent=n->d.parent_id;
    while(parent){Node*p=node_find(r,parent);if(!p)return uim_ERR_STATE;if(!p->d.visible||!p->d.enabled)return uim_ERR_STATE;parent=p->d.parent_id;if(++guard>r->node_count)return uim_ERR_STATE;}
    memset(&e,0,sizeof(e));e.type=uim_EVENT_ACTION;e.node_id=id;e.action_id=n->d.action_id;e.value=payload;
    rc=event_push(r,e);return rc;
}
uim_Result uim_setting_set_pending(UIM_ComponentManager*r,uim_PropertyId id,uim_Value v){if(!r||!id||!value_valid(v))return uim_ERR_ARGUMENT;return set_pending_internal(r,id,v);}
uim_Result uim_setting_set_pending_batch(UIM_ComponentManager*r,const uim_PropertyId*ids,const uim_Value*values,size_t count){
    size_t i,j;Setting**targets;uim_Value*copies;
    if(!r||(!ids&&count)||(!values&&count))return uim_ERR_ARGUMENT;
    if(!count)return uim_OK;
    if(count>SIZE_MAX/sizeof(*targets)||count>SIZE_MAX/sizeof(*copies))return uim_ERR_RANGE;
    targets=(Setting**)calloc(count,sizeof(*targets));copies=(uim_Value*)calloc(count,sizeof(*copies));
    if(!targets||!copies){free(targets);free(copies);return uim_ERR_NOMEM;}
    for(i=0;i<count;i++){
        if(!ids[i]||!value_valid(values[i]))goto argument_error;
        for(j=0;j<i;j++)if(ids[j]==ids[i])goto exists_error;
        targets[i]=setting_find(r,ids[i]);if(!targets[i])goto not_found_error;
        if(targets[i]->pending.type!=values[i].type)goto type_error;
        if(!uim_value_equal(targets[i]->pending,values[i])&&targets[i]->revision==UINT64_MAX)goto range_error;
        if(value_copy(&copies[i],values[i])!=uim_OK)goto nomem_error;
    }
    for(i=0;i<count;i++)if(!uim_value_equal(targets[i]->pending,copies[i])){
        value_free(&targets[i]->pending);targets[i]->pending=copies[i];memset(&copies[i],0,sizeof(copies[i]));targets[i]->revision++;
    }
    free(targets);free(copies);return uim_OK;
argument_error:for(j=0;j<count;j++)value_free(&copies[j]);free(targets);free(copies);return uim_ERR_ARGUMENT;
exists_error:for(j=0;j<count;j++)value_free(&copies[j]);free(targets);free(copies);return uim_ERR_EXISTS;
not_found_error:for(j=0;j<count;j++)value_free(&copies[j]);free(targets);free(copies);return uim_ERR_NOT_FOUND;
type_error:for(j=0;j<count;j++)value_free(&copies[j]);free(targets);free(copies);return uim_ERR_TYPE;
range_error:for(j=0;j<count;j++)value_free(&copies[j]);free(targets);free(copies);return uim_ERR_RANGE;
nomem_error:for(j=0;j<count;j++)value_free(&copies[j]);free(targets);free(copies);return uim_ERR_NOMEM;
}
uim_Result uim_setting_get_state(const UIM_ComponentManager*r,uim_PropertyId id,uim_SettingState*out){const Setting*s;if(!r||!id||!out)return uim_ERR_ARGUMENT;s=setting_find_c(r,id);if(!s)return uim_ERR_NOT_FOUND;out->initial=s->initial;out->committed=s->committed;out->pending=s->pending;out->dirty=!uim_value_equal(s->pending,s->committed);out->revision=s->revision;return uim_OK;}
uim_Result uim_setting_reset(UIM_ComponentManager*r,uim_PropertyId id){Setting*s;uim_Value cp={0};if(!r||!id)return uim_ERR_ARGUMENT;s=setting_find(r,id);if(!s)return uim_ERR_NOT_FOUND;if(uim_value_equal(s->pending,s->initial))return uim_OK;if(s->revision==UINT64_MAX)return uim_ERR_RANGE;if(value_copy(&cp,s->initial)!=uim_OK)return uim_ERR_NOMEM;value_free(&s->pending);s->pending=cp;s->revision++;return uim_OK;}
uim_Result uim_setting_cancel(UIM_ComponentManager*r,uim_PropertyId id){Setting*s;uim_Value cp={0};if(!r||!id)return uim_ERR_ARGUMENT;s=setting_find(r,id);if(!s)return uim_ERR_NOT_FOUND;if(uim_value_equal(s->pending,s->committed))return uim_OK;if(s->revision==UINT64_MAX)return uim_ERR_RANGE;if(value_copy(&cp,s->committed)!=uim_OK)return uim_ERR_NOMEM;value_free(&s->pending);s->pending=cp;s->revision++;return uim_OK;}
uim_Result uim_apply_request_for(UIM_ComponentManager*r,const uim_PropertyId*ids,size_t count,uint64_t*out){size_t i,j;Setting**targets;uim_Value*copies;if(!r||!out||(!ids&&count))return uim_ERR_ARGUMENT;if(r->apply_pending)return uim_ERR_STATE;if(r->next_serial==0||r->next_serial==UINT64_MAX)return uim_ERR_RANGE;if(count>SIZE_MAX/sizeof(*targets)||count>SIZE_MAX/sizeof(*copies))return uim_ERR_RANGE;targets=count?(Setting**)calloc(count,sizeof(*targets)):NULL;copies=count?(uim_Value*)calloc(count,sizeof(*copies)):NULL;if(count&&(!targets||!copies)){free(targets);free(copies);return uim_ERR_NOMEM;}for(i=0;i<count;i++){if(!ids[i]){while(i)value_free(&copies[--i]);free(targets);free(copies);return uim_ERR_ARGUMENT;}for(j=0;j<i;j++)if(ids[j]==ids[i]){while(i)value_free(&copies[--i]);free(targets);free(copies);return uim_ERR_EXISTS;}targets[i]=setting_find(r,ids[i]);if(!targets[i]){while(i)value_free(&copies[--i]);free(targets);free(copies);return uim_ERR_NOT_FOUND;}if(value_copy(&copies[i],targets[i]->pending)!=uim_OK){while(i)value_free(&copies[--i]);free(targets);free(copies);return uim_ERR_NOMEM;}}for(i=0;i<r->setting_count;i++){value_free(&r->settings[i].snapshot);r->settings[i].snapshot_valid=false;}for(i=0;i<count;i++){targets[i]->snapshot=copies[i];memset(&copies[i],0,sizeof(copies[i]));targets[i]->snapshot_valid=true;}free(targets);free(copies);r->active_serial=r->next_serial++;r->apply_pending=true;*out=r->active_serial;return uim_OK;}
uim_Result uim_apply_request(UIM_ComponentManager*r,uint64_t*out){uim_PropertyId*ids;size_t i;uim_Result rc;if(!r||!out)return uim_ERR_ARGUMENT;if(r->setting_count>SIZE_MAX/sizeof(*ids))return uim_ERR_RANGE;ids=r->setting_count?(uim_PropertyId*)malloc(r->setting_count*sizeof(*ids)):NULL;if(r->setting_count&&!ids)return uim_ERR_NOMEM;for(i=0;i<r->setting_count;i++)ids[i]=r->settings[i].id;rc=uim_apply_request_for(r,ids,r->setting_count,out);free(ids);return rc;}
uim_Result uim_apply_confirm(UIM_ComponentManager*r,uint64_t serial,bool success){
    size_t i;uim_Value*commits=NULL,*props=NULL;
    if(!r||!serial)return uim_ERR_ARGUMENT;
    if(!r->apply_pending||serial!=r->active_serial)return uim_ERR_STALE;
    if(success){
        if(r->setting_count>SIZE_MAX/sizeof(*commits))return uim_ERR_RANGE;
        commits=r->setting_count?(uim_Value*)calloc(r->setting_count,sizeof(*commits)):NULL;
        props=r->setting_count?(uim_Value*)calloc(r->setting_count,sizeof(*props)):NULL;
        if(r->setting_count&&(!commits||!props)){free(commits);free(props);return uim_ERR_NOMEM;}
        for(i=0;i<r->setting_count;i++){
            Setting*s=&r->settings[i];
            if(!s->snapshot_valid)continue;
            if(!uim_value_equal(s->committed,s->snapshot)&&s->revision==UINT64_MAX){
                size_t j;for(j=0;j<r->setting_count;j++){value_free(&commits[j]);value_free(&props[j]);}
                free(commits);free(props);return uim_ERR_RANGE;
            }
            if(value_copy(&commits[i],s->snapshot)!=uim_OK){
                size_t j;for(j=0;j<r->setting_count;j++){value_free(&commits[j]);value_free(&props[j]);}
                free(commits);free(props);return uim_ERR_NOMEM;
            }
            if(value_copy(&props[i],s->snapshot)!=uim_OK){
                size_t j;for(j=0;j<r->setting_count;j++){value_free(&commits[j]);value_free(&props[j]);}
                free(commits);free(props);return uim_ERR_NOMEM;
            }
        }
        for(i=0;i<r->setting_count;i++){
            Setting*s=&r->settings[i];Property*p=prop_find(r,s->id);bool changed;
            if(!s->snapshot_valid)continue;
            changed=!uim_value_equal(s->committed,commits[i]);
            value_free(&s->committed);s->committed=commits[i];memset(&commits[i],0,sizeof(commits[i]));
            if(p){value_free(&p->value);p->value=props[i];memset(&props[i],0,sizeof(props[i]));}
            if(changed)s->revision++;
        }
        for(i=0;i<r->setting_count;i++){value_free(&commits[i]);value_free(&props[i]);}
        free(commits);free(props);
    }
    for(i=0;i<r->setting_count;i++){value_free(&r->settings[i].snapshot);r->settings[i].snapshot_valid=false;}
    r->apply_pending=false;r->active_serial=0;return uim_OK;
}
uim_Result uim_apply_is_pending(const UIM_ComponentManager*r,uint64_t*out){if(!r||!out)return uim_ERR_ARGUMENT;if(!r->apply_pending)return uim_ERR_NOT_FOUND;*out=r->active_serial;return uim_OK;}
uim_Result uim_event_poll(UIM_ComponentManager*r,uim_Event*out){if(!r||!out)return uim_ERR_ARGUMENT;if(!r->event_count)return uim_ERR_NOT_FOUND;*out=r->events[0];memmove(r->events,r->events+1,(r->event_count-1)*sizeof(uim_Event));r->event_count--;return uim_OK;}
void uim_event_dispose(uim_Event*e){if(!e)return;if(e->value.type==uim_VALUE_STRING)value_free(&e->value);memset(e,0,sizeof(*e));}
uim_Result uim_event_clear(UIM_ComponentManager*r){size_t i;if(!r)return uim_ERR_ARGUMENT;for(i=0;i<r->event_count;i++)if(r->events[i].value.type==uim_VALUE_STRING)value_free(&r->events[i].value);r->event_count=0;return uim_OK;}
static Layout *layout_find(UIM_ComponentManager*r,const char*n){size_t i;for(i=0;i<r->layout_count;i++)if(strcmp(r->layouts[i].name,n)==0)return &r->layouts[i];return NULL;}
static Preset *preset_find(UIM_ComponentManager*r,const char*n){size_t i;for(i=0;i<r->preset_count;i++)if(strcmp(r->presets[i].name,n)==0)return &r->presets[i];return NULL;}
uim_Result uim_layout_save(UIM_ComponentManager*r,const char*name,uim_PageId page){size_t i,n=0,k=0;Layout*l;uim_LayoutNode*items;uim_Result rc;if(!r||!name||!name[0]||!page_find(r,page))return uim_ERR_ARGUMENT;for(i=0;i<r->node_count;i++)if(r->nodes[i].d.page_id==page)n++;items=n?(uim_LayoutNode*)calloc(n,sizeof(*items)):NULL;if(n&&!items)return uim_ERR_NOMEM;for(i=0;i<r->node_count;i++)if(r->nodes[i].d.page_id==page){Node*x=&r->nodes[i];Node*parent=x->d.parent_id?node_find(r,x->d.parent_id):NULL;items[k].stable_key=x->stable_key?dup_n(x->stable_key,strlen(x->stable_key)):NULL;items[k].parent_stable_key=(parent&&parent->stable_key)?dup_n(parent->stable_key,strlen(parent->stable_key)):dup_n("",0);if((x->stable_key&&!items[k].stable_key)||!items[k].parent_stable_key){layout_nodes_free(items,n);return uim_ERR_NOMEM;}items[k].node_id=x->id;items[k].parent_id=x->d.parent_id;items[k].rect=x->d.local_rect;items[k].layout=x->d.layout;items[k].visible=x->d.visible;items[k].enabled=x->d.enabled;items[k].z_order=x->d.z_order;items[k].child_order=x->d.child_order;k++;}l=layout_find(r,name);if(!l){if(!grow((void**)&r->layouts,&r->layout_cap,r->layout_count+1,sizeof(Layout))){layout_nodes_free(items,n);return uim_ERR_NOMEM;}l=&r->layouts[r->layout_count++];memset(l,0,sizeof(*l));l->name=dup_n(name,strlen(name));if(!l->name){r->layout_count--;layout_nodes_free(items,n);return uim_ERR_NOMEM;}}else layout_nodes_free(l->nodes,l->count);l->page=page;l->nodes=items;l->count=n;rc=uim_OK;return rc;}
uim_Result uim_layout_get(const UIM_ComponentManager*r,const char*name,uim_LayoutSnapshot*out){size_t i;if(!r||!name||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->layout_count;i++)if(strcmp(r->layouts[i].name,name)==0){out->page_id=r->layouts[i].page;out->nodes=r->layouts[i].nodes;out->node_count=r->layouts[i].count;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_layout_load(UIM_ComponentManager*r,const char*name){Layout*l;size_t i; if(!r||!name)return uim_ERR_ARGUMENT;l=layout_find(r,name);if(!l)return uim_ERR_NOT_FOUND;if(!page_find(r,l->page))return uim_ERR_NOT_FOUND;for(i=0;i<l->count;i++){Node*n=node_find(r,l->nodes[i].node_id);if(!n||n->d.page_id!=l->page)return uim_ERR_STALE;}for(i=0;i<l->count;i++){Node*n=node_find(r,l->nodes[i].node_id);n->d.parent_id=l->nodes[i].parent_id;n->d.local_rect=l->nodes[i].rect;n->d.layout=l->nodes[i].layout;n->d.visible=l->nodes[i].visible;n->d.enabled=l->nodes[i].enabled;n->d.z_order=l->nodes[i].z_order;n->d.child_order=l->nodes[i].child_order;}return uim_OK;}
uim_Result uim_preset_save(UIM_ComponentManager*r,const char*name){size_t i,k=0;Preset*p;uim_PresetEntry*e;char*new_name;if(!r||!name||!name[0])return uim_ERR_ARGUMENT;new_name=dup_n(name,strlen(name));if(!new_name)return uim_ERR_NOMEM;e=r->setting_count?(uim_PresetEntry*)calloc(r->setting_count,sizeof(*e)):NULL;if(r->setting_count&&!e){free(new_name);return uim_ERR_NOMEM;}for(i=0;i<r->setting_count;i++){e[k].property_id=r->settings[i].id;if(value_copy(&e[k].value,r->settings[i].pending)!=uim_OK){while(k)value_free(&e[--k].value);free(e);free(new_name);return uim_ERR_NOMEM;}k++;}p=preset_find(r,name);if(!p){if(!grow((void**)&r->presets,&r->preset_cap,r->preset_count+1,sizeof(Preset))){for(i=0;i<k;i++)value_free(&e[i].value);free(e);free(new_name);return uim_ERR_NOMEM;}p=&r->presets[r->preset_count++];memset(p,0,sizeof(*p));}else preset_free(p);p->name=new_name;p->entries=e;p->count=k;return uim_OK;}
uim_Result uim_preset_get(const UIM_ComponentManager*r,const char*name,uim_PresetSnapshot*out){size_t i;if(!r||!name||!out)return uim_ERR_ARGUMENT;for(i=0;i<r->preset_count;i++)if(strcmp(r->presets[i].name,name)==0){out->entries=r->presets[i].entries;out->entry_count=r->presets[i].count;return uim_OK;}return uim_ERR_NOT_FOUND;}
uim_Result uim_preset_load(UIM_ComponentManager*r,const char*name){Preset*p;size_t i;uim_Value*tmp;Setting**targets;if(!r||!name)return uim_ERR_ARGUMENT;p=preset_find(r,name);if(!p)return uim_ERR_NOT_FOUND;if(p->count>SIZE_MAX/sizeof(*tmp)||p->count>SIZE_MAX/sizeof(*targets))return uim_ERR_RANGE;tmp=p->count?(uim_Value*)calloc(p->count,sizeof(*tmp)):NULL;targets=p->count?(Setting**)calloc(p->count,sizeof(*targets)):NULL;if(p->count&&(!tmp||!targets)){free(tmp);free(targets);return uim_ERR_NOMEM;}for(i=0;i<p->count;i++){targets[i]=setting_find(r,p->entries[i].property_id);if(!targets[i]){while(i)value_free(&tmp[--i]);free(tmp);free(targets);return uim_ERR_NOT_FOUND;}if(targets[i]->pending.type!=p->entries[i].value.type){while(i)value_free(&tmp[--i]);free(tmp);free(targets);return uim_ERR_TYPE;}if(!uim_value_equal(targets[i]->pending,p->entries[i].value)&&targets[i]->revision==UINT64_MAX){while(i)value_free(&tmp[--i]);free(tmp);free(targets);return uim_ERR_RANGE;}if(value_copy(&tmp[i],p->entries[i].value)!=uim_OK){while(i)value_free(&tmp[--i]);free(tmp);free(targets);return uim_ERR_NOMEM;}}for(i=0;i<p->count;i++)if(!uim_value_equal(targets[i]->pending,tmp[i])){value_free(&targets[i]->pending);targets[i]->pending=tmp[i];memset(&tmp[i],0,sizeof(tmp[i]));targets[i]->revision++;}for(i=0;i<p->count;i++)value_free(&tmp[i]);free(tmp);free(targets);return uim_OK;}


/* Multi-component document instances. Runtime handles are deliberately absent:
 * these records are stable document identities and serialize independently. */
uim_Result uim_component_instance_attach(UIM_ComponentManager *manager, uim_NodeId node_id,
                                         uim_ComponentTypeId type_id, uint32_t schema_version,
                                         uim_ComponentInstanceId *out_instance_id)
{
    ComponentInstance *entry;
    uim_NodeInfo node;
    if (!manager || !node_id || !type_id || !schema_version || !out_instance_id)
        return uim_ERR_ARGUMENT;
    *out_instance_id = uim_COMPONENT_INSTANCE_INVALID;
    if (uim_node_get(manager, node_id, &node) != uim_OK) return uim_ERR_NOT_FOUND;
    if (!manager->next_component_instance || manager->next_component_instance == UINT64_MAX)
        return uim_ERR_RANGE;
    if (!grow((void **)&manager->component_instances,
              &manager->component_instance_cap,
              manager->component_instance_count + 1u,
              sizeof(*manager->component_instances))) return uim_ERR_NOMEM;
    entry = &manager->component_instances[manager->component_instance_count++];
    memset(entry, 0, sizeof(*entry));
    entry->info.instance_id = manager->next_component_instance++;
    entry->info.node_id = node_id;
    entry->info.type_id = type_id;
    entry->info.schema_version = schema_version;
    *out_instance_id = entry->info.instance_id;
    return uim_OK;
}

uim_Result uim_component_instance_restore(UIM_ComponentManager *manager,
                                          const uim_ComponentInstanceInfo *info)
{
    ComponentInstance *entry;
    uim_NodeInfo node;
    size_t i;
    if (!manager || !info || !info->instance_id || !info->node_id ||
        !info->type_id || !info->schema_version) return uim_ERR_ARGUMENT;
    if (info->instance_id == UINT64_MAX) return uim_ERR_RANGE;
    if (uim_node_get(manager, info->node_id, &node) != uim_OK) return uim_ERR_NOT_FOUND;
    for (i = 0; i < manager->component_instance_count; ++i)
        if (manager->component_instances[i].info.instance_id == info->instance_id)
            return uim_ERR_EXISTS;
    if (!grow((void **)&manager->component_instances,
              &manager->component_instance_cap,
              manager->component_instance_count + 1u,
              sizeof(*manager->component_instances))) return uim_ERR_NOMEM;
    entry = &manager->component_instances[manager->component_instance_count++];
    memset(entry, 0, sizeof(*entry));
    entry->info = *info;
    if (manager->next_component_instance <= info->instance_id)
        manager->next_component_instance = info->instance_id + 1u;
    return uim_OK;
}

uim_Result uim_component_instance_detach(UIM_ComponentManager *manager,
                                         uim_ComponentInstanceId instance_id)
{
    size_t i;
    if (!manager || !instance_id) return uim_ERR_ARGUMENT;
    for (i = 0; i < manager->component_instance_count; ++i) {
        if (manager->component_instances[i].info.instance_id == instance_id) {
            component_instance_remove_at(manager, i);
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

uim_Result uim_component_instance_get(const UIM_ComponentManager *manager,
                                      uim_ComponentInstanceId instance_id,
                                      uim_ComponentInstanceInfo *out_info)
{
    size_t i;
    if (!manager || !instance_id || !out_info) return uim_ERR_ARGUMENT;
    for (i = 0; i < manager->component_instance_count; ++i) {
        if (manager->component_instances[i].info.instance_id == instance_id) {
            *out_info = manager->component_instances[i].info;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

uim_Result uim_node_component_count(const UIM_ComponentManager *manager, uim_NodeId node_id,
                                    size_t *out_count)
{
    size_t i, count = 0;
    uim_NodeInfo node;
    if (!manager || !node_id || !out_count) return uim_ERR_ARGUMENT;
    if (uim_node_get(manager, node_id, &node) != uim_OK) return uim_ERR_NOT_FOUND;
    for (i = 0; i < manager->component_instance_count; ++i)
        if (manager->component_instances[i].info.node_id == node_id) ++count;
    *out_count = count;
    return uim_OK;
}

uim_Result uim_node_component_get_at(const UIM_ComponentManager *manager, uim_NodeId node_id,
                                     size_t index, uim_ComponentInstanceInfo *out_info)
{
    size_t i, found = 0;
    uim_NodeInfo node;
    if (!manager || !node_id || !out_info) return uim_ERR_ARGUMENT;
    if (uim_node_get(manager, node_id, &node) != uim_OK) return uim_ERR_NOT_FOUND;
    for (i = 0; i < manager->component_instance_count; ++i) {
        if (manager->component_instances[i].info.node_id == node_id) {
            if (found++ == index) { *out_info = manager->component_instances[i].info; return uim_OK; }
        }
    }
    return uim_ERR_NOT_FOUND;
}

uim_Result uim_component_instance_set_property(UIM_ComponentManager *manager,
                                               uim_ComponentInstanceId instance_id,
                                               uim_PropertyId property_id,
                                               uim_Value value)
{
    size_t i, j;
    uim_Value copy;
    ComponentInstance *entry = NULL;
    uim_Result rc;
    if (!manager || !instance_id || !property_id || !value_valid(value)) return uim_ERR_ARGUMENT;
    for (i = 0; i < manager->component_instance_count; ++i)
        if (manager->component_instances[i].info.instance_id == instance_id) {
            entry = &manager->component_instances[i]; break;
        }
    if (!entry) return uim_ERR_NOT_FOUND;
    rc = value_copy(&copy, value);
    if (rc != uim_OK) return rc;
    for (j = 0; j < entry->property_count; ++j) {
        if (entry->properties[j].property_id == property_id) {
            value_free(&entry->properties[j].value);
            entry->properties[j].value = copy;
            return uim_OK;
        }
    }
    if (!grow((void **)&entry->properties, &entry->property_cap,
              entry->property_count + 1u, sizeof(*entry->properties))) {
        value_free(&copy);
        return uim_ERR_NOMEM;
    }
    entry->properties[entry->property_count].property_id = property_id;
    entry->properties[entry->property_count].value = copy;
    entry->property_count++;
    return uim_OK;
}

uim_Result uim_component_instance_get_property(const UIM_ComponentManager *manager,
                                               uim_ComponentInstanceId instance_id,
                                               uim_PropertyId property_id,
                                               uim_Value *out_value)
{
    size_t i, j;
    if (!manager || !instance_id || !property_id || !out_value) return uim_ERR_ARGUMENT;
    for (i = 0; i < manager->component_instance_count; ++i) {
        const ComponentInstance *entry = &manager->component_instances[i];
        if (entry->info.instance_id != instance_id) continue;
        for (j = 0; j < entry->property_count; ++j) {
            if (entry->properties[j].property_id == property_id) {
                *out_value = entry->properties[j].value;
                return uim_OK;
            }
        }
        return uim_ERR_NOT_FOUND;
    }
    return uim_ERR_NOT_FOUND;
}

uim_Result uim_component_instance_property_count(const UIM_ComponentManager *manager,
                                                 uim_ComponentInstanceId instance_id,
                                                 size_t *out_count)
{
    size_t i;
    if (!manager || !instance_id || !out_count) return uim_ERR_ARGUMENT;
    for (i = 0; i < manager->component_instance_count; ++i) {
        if (manager->component_instances[i].info.instance_id == instance_id) {
            *out_count = manager->component_instances[i].property_count;
            return uim_OK;
        }
    }
    return uim_ERR_NOT_FOUND;
}

uim_Result uim_component_instance_property_get_at(const UIM_ComponentManager *manager,
                                                  uim_ComponentInstanceId instance_id,
                                                  size_t index,
                                                  uim_PropertyId *out_property_id,
                                                  uim_Value *out_value)
{
    size_t i;
    if (!manager || !instance_id || !out_property_id || !out_value) return uim_ERR_ARGUMENT;
    for (i = 0; i < manager->component_instance_count; ++i) {
        const ComponentInstance *entry = &manager->component_instances[i];
        if (entry->info.instance_id != instance_id) continue;
        if (index >= entry->property_count) return uim_ERR_NOT_FOUND;
        *out_property_id = entry->properties[index].property_id;
        *out_value = entry->properties[index].value;
        return uim_OK;
    }
    return uim_ERR_NOT_FOUND;
}
