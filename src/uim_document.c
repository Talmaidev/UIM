/* UIM document layer: pages and nodes.
 *
 * Physically separate from zcsr_uim_component.  The public API remains in
 * uim_component_manager.h for source compatibility, but these symbols are now
 * compiled only into zcsr_uim_document.  Both translation units include the
 * same private layout header so the manager remains one runtime object.
 */
#include "uim_component_manager.h"
#include "uim_document_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool grow(void **p,size_t *cap,size_t need,size_t elem){size_t n;void*q;if(need<=*cap)return true;n=*cap?*cap:8;while(n<need){if(n>SIZE_MAX/2)return false;n*=2;}if(elem&&n>SIZE_MAX/elem)return false;q=realloc(*p,n*elem);if(!q)return false;*p=q;*cap=n;return true;}
static char *dup_n(const char*s,size_t n){char*p;if(!s&&n)return NULL;p=(char*)malloc(n+1);if(!p)return NULL;if(n)memcpy(p,s,n);p[n]=0;return p;}
static void value_free(uim_Value*v){if(!v)return;if(v->type==uim_VALUE_STRING)free((void*)v->as.string.data);memset(v,0,sizeof(*v));}
static bool rect_ok(uim_Rect r){return isfinite(r.x)&&isfinite(r.y)&&isfinite(r.width)&&isfinite(r.height)&&r.width>=0&&r.height>=0;}
static bool transform3_ok(const uim_Transform3*t){size_t i;if(!t)return false;for(i=0;i<3;i++)if(!isfinite(t->position[i])||!isfinite(t->rotation[i])||!isfinite(t->scale[i]))return false;return true;}
static bool layout_constraints_ok(uim_LayoutConstraints c){return !c.enabled||(isfinite(c.anchor_min_x)&&isfinite(c.anchor_min_y)&&isfinite(c.anchor_max_x)&&isfinite(c.anchor_max_y)&&c.anchor_min_x>=0.0f&&c.anchor_min_x<=1.0f&&c.anchor_min_y>=0.0f&&c.anchor_min_y<=1.0f&&c.anchor_max_x>=0.0f&&c.anchor_max_x<=1.0f&&c.anchor_max_y>=0.0f&&c.anchor_max_y<=1.0f&&c.anchor_min_x<=c.anchor_max_x&&c.anchor_min_y<=c.anchor_max_y&&isfinite(c.offset_left)&&isfinite(c.offset_top)&&isfinite(c.offset_right)&&isfinite(c.offset_bottom));}
static Page *page_find(UIM_ComponentManager*r,uim_PageId id){size_t i;for(i=0;i<r->page_count;i++)if(r->pages[i].id==id)return &r->pages[i];return NULL;}
static Node *node_find(UIM_ComponentManager*r,uim_NodeId id){size_t i;for(i=0;i<r->node_count;i++)if(r->nodes[i].id==id)return &r->nodes[i];return NULL;}
static const Node *node_find_c(const UIM_ComponentManager*r,uim_NodeId id){size_t i;for(i=0;i<r->node_count;i++)if(r->nodes[i].id==id)return &r->nodes[i];return NULL;}
static void node_info(const Node*n,uim_NodeInfo*out);
static void remove_binding_for_node(UIM_ComponentManager*r,Node*n){size_t i;if(!n->binding)return;for(i=0;i<r->binding_count;i++)if(r->bindings[i].id==n->binding){memmove(&r->bindings[i],&r->bindings[i+1],(r->binding_count-i-1)*sizeof(Binding));r->binding_count--;break;}n->binding=0;}
static void component_instance_remove_at(UIM_ComponentManager*r,size_t i){ComponentInstance*c=&r->component_instances[i];size_t j;for(j=0;j<c->property_count;j++)value_free(&c->properties[j].value);free(c->properties);memmove(c,c+1,(r->component_instance_count-i-1)*sizeof(*c));r->component_instance_count--;}
static void component_instances_remove_for_node(UIM_ComponentManager*r,uim_NodeId node_id){size_t i=0;while(i<r->component_instance_count){if(r->component_instances[i].info.node_id==node_id)component_instance_remove_at(r,i);else i++;}}
static void node_remove_index(UIM_ComponentManager*r,size_t i){Node*n=&r->nodes[i];component_instances_remove_for_node(r,n->id);remove_binding_for_node(r,n);free(n->stable_key);free(n->config);memmove(n,n+1,(r->node_count-i-1)*sizeof(Node));r->node_count--;}
static void node_destroy_recursive(UIM_ComponentManager*r,uim_NodeId id){size_t i=0;while(i<r->node_count){if(r->nodes[i].d.parent_id==id){uim_NodeId child=r->nodes[i].id;node_destroy_recursive(r,child);i=0;}else i++;}for(i=0;i<r->node_count;i++)if(r->nodes[i].id==id){node_remove_index(r,i);return;}}
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
