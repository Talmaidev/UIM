#include "uim_component_manager_settings.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool positive(float x) { return isfinite(x) && x > 0.0f; }
static bool nonnegative(float x) { return isfinite(x) && x >= 0.0f; }

static uim_Result add_node(UIM_ComponentManager *r, uim_PageId page, uim_NodeId parent,
                           uim_ComponentId component, const char *key, uim_Rect rect,
                           uim_NodeId *out, const void *config, size_t config_size) {
    uim_NodeDesc d;
    memset(&d, 0, sizeof(d));
    d.stable_key = key; d.page_id = page; d.parent_id = parent;
    d.component_id = component; d.local_rect = rect; d.visible = true; d.enabled = true;
    d.component_config = config; d.component_config_size = config_size;
    return uim_node_create(r, &d, out);
}

static bool valid_desc(const uim_SettingsWindowDesc *d) {
    size_t i, j;
    if (!d || !d->page_key || !d->page_key[0] || !d->window_component_id ||
        !d->panel_component_id || !d->scroll_component_id || !d->label_component_id ||
        !d->button_component_id || !positive(d->width) || !positive(d->height) ||
        !positive(d->header_height) || !positive(d->sidebar_width) ||
        !positive(d->footer_height) || !nonnegative(d->padding) ||
        !positive(d->row_height) || !nonnegative(d->row_gap) ||
        !positive(d->action_button_width) || !positive(d->action_button_height) ||
        (!d->items && d->item_count) || (!d->categories && d->category_count) ||
        (d->category_count && !d->category_component_id) ||
        d->header_height + d->footer_height >= d->height ||
        d->sidebar_width >= d->width || d->padding * 2.0f >= d->width - d->sidebar_width ||
        d->action_button_width*3.0f+d->padding*4.0f>d->width ||
        d->action_button_height>d->footer_height || d->padding*2.0f>=d->height-d->header_height-d->footer_height ||
        d->category_count*d->row_height>d->height-d->header_height-d->footer_height)
        return false;
    for (i=0;i<d->category_count;i++) {
        const uim_SettingsCategoryDesc *cat=&d->categories[i];
        if (!cat->stable_key || !cat->stable_key[0] || strlen(cat->stable_key)>450 ||
            (!cat->component_config && cat->component_config_size)) return false;
        for(j=0;j<i;j++) if(strcmp(d->categories[j].stable_key,cat->stable_key)==0)return false;
    }
    for (i=0;i<d->item_count;i++) {
        const uim_SettingsItemDesc *it=&d->items[i];
        if (!it->stable_key || !it->stable_key[0] || strlen(it->stable_key)>450 || !it->property_id ||
            !it->label_component_id || !it->control_component_id ||
            (!it->label_config && it->label_config_size) ||
            (!it->control_config && it->control_config_size) ||
            (d->category_count && it->category_index>=d->category_count) ||
            (!d->category_count && it->category_index!=0)) return false;
        for(j=0;j<i;j++) if(strcmp(d->items[j].stable_key,it->stable_key)==0 ||
                            d->items[j].property_id==it->property_id) return false;
    }
    return true;
}

void uim_settings_window_dispose(uim_SettingsWindow *w) {
    if(!w)return;
    free(w->row_nodes);free(w->label_nodes);free(w->control_nodes);free(w->property_ids);free(w->row_category_indices);free(w->category_nodes);
    memset(w,0,sizeof(*w));
}
uim_Result uim_settings_window_destroy(UIM_ComponentManager *r, uim_SettingsWindow *w) {
    uim_Result rc;
    if(!r||!w||!w->page_id)return uim_ERR_ARGUMENT;
    rc=uim_page_destroy(r,w->page_id);
    if(rc==uim_OK)uim_settings_window_dispose(w);
    return rc;
}

uim_Result uim_settings_window_build(UIM_ComponentManager *r,
                                     const uim_SettingsWindowDesc *d,
                                     uim_SettingsWindow *out) {
    uim_SettingsWindow w; uim_Result rc; size_t i;
    float body_h, content_x, content_w, row_w;
    char key[512];
    if(!r||!out||!valid_desc(d))return uim_ERR_ARGUMENT;
    memset(out,0,sizeof(*out));
    memset(&w,0,sizeof(w));
    if(d->item_count){
        w.row_nodes=(uim_NodeId*)calloc(d->item_count,sizeof(uim_NodeId));
        w.label_nodes=(uim_NodeId*)calloc(d->item_count,sizeof(uim_NodeId));
        w.control_nodes=(uim_NodeId*)calloc(d->item_count,sizeof(uim_NodeId));
        w.property_ids=(uim_PropertyId*)calloc(d->item_count,sizeof(uim_PropertyId));
        if(!w.row_nodes||!w.label_nodes||!w.control_nodes||!w.property_ids){uim_settings_window_dispose(&w);return uim_ERR_NOMEM;}
    }
    if(d->item_count){w.row_category_indices=(uint32_t*)calloc(d->item_count,sizeof(uint32_t));if(!w.row_category_indices){uim_settings_window_dispose(&w);return uim_ERR_NOMEM;}}
    if(d->category_count){w.category_nodes=(uim_NodeId*)calloc(d->category_count,sizeof(uim_NodeId));if(!w.category_nodes){uim_settings_window_dispose(&w);return uim_ERR_NOMEM;}}
    w.item_count=d->item_count;w.category_count=d->category_count;w.active_category=0;
    rc=uim_page_create(r,d->page_key,&w.page_id);if(rc!=uim_OK)goto fail;
    body_h=d->height-d->header_height-d->footer_height;
    content_x=d->sidebar_width;
    content_w=d->width-content_x;
    row_w=content_w-d->padding*2.0f;
    if((rc=add_node(r,w.page_id,0,d->window_component_id,"settings.window",(uim_Rect){0,0,d->width,d->height},&w.window_node,NULL,0))!=uim_OK)goto fail;
    if((rc=add_node(r,w.page_id,w.window_node,d->panel_component_id,"settings.header",(uim_Rect){0,0,d->width,d->header_height},&w.header_node,NULL,0))!=uim_OK)goto fail;
    if((rc=add_node(r,w.page_id,w.window_node,d->panel_component_id,"settings.sidebar",(uim_Rect){0,d->header_height,d->sidebar_width,body_h},&w.sidebar_node,NULL,0))!=uim_OK)goto fail;
    if((rc=add_node(r,w.page_id,w.window_node,d->panel_component_id,"settings.content",(uim_Rect){content_x,d->header_height,content_w,body_h},&w.content_node,NULL,0))!=uim_OK)goto fail;
    if((rc=add_node(r,w.page_id,w.content_node,d->scroll_component_id,"settings.scroll",(uim_Rect){d->padding,d->padding,row_w,body_h-d->padding*2.0f},&w.scroll_node,NULL,0))!=uim_OK)goto fail;
    if((rc=add_node(r,w.page_id,w.window_node,d->panel_component_id,"settings.footer",(uim_Rect){0,d->height-d->footer_height,d->width,d->footer_height},&w.footer_node,NULL,0))!=uim_OK)goto fail;
    {float y=d->footer_height*0.5f-d->action_button_height*0.5f;float gap=d->padding;float right=d->width-d->padding;
     if((rc=add_node(r,w.page_id,w.footer_node,d->button_component_id,"settings.action.apply",(uim_Rect){right-d->action_button_width,y,d->action_button_width,d->action_button_height},&w.apply_button,NULL,0))!=uim_OK)goto fail;
     if((rc=uim_node_set_action_id(r,w.apply_button,(uim_ActionId)uim_SETTINGS_APPLY))!=uim_OK)goto fail;
     right-=d->action_button_width+gap;
     if((rc=add_node(r,w.page_id,w.footer_node,d->button_component_id,"settings.action.cancel",(uim_Rect){right-d->action_button_width,y,d->action_button_width,d->action_button_height},&w.cancel_button,NULL,0))!=uim_OK)goto fail;
     if((rc=uim_node_set_action_id(r,w.cancel_button,(uim_ActionId)uim_SETTINGS_CANCEL))!=uim_OK)goto fail;
     right-=d->action_button_width+gap;
     if((rc=add_node(r,w.page_id,w.footer_node,d->button_component_id,"settings.action.reset",(uim_Rect){right-d->action_button_width,y,d->action_button_width,d->action_button_height},&w.reset_button,NULL,0))!=uim_OK)goto fail;
     if((rc=uim_node_set_action_id(r,w.reset_button,(uim_ActionId)uim_SETTINGS_RESET))!=uim_OK)goto fail;}
    for(i=0;i<d->category_count;i++){
        (void)snprintf(key,sizeof(key),"settings.category.%s",d->categories[i].stable_key);
        if((rc=add_node(r,w.page_id,w.sidebar_node,d->category_component_id,key,(uim_Rect){0,(float)i*d->row_height,d->sidebar_width,d->row_height},&w.category_nodes[i],d->categories[i].component_config,d->categories[i].component_config_size))!=uim_OK)goto fail;
    }
    for(i=0;i<d->item_count;i++){
        w.property_ids[i]=d->items[i].property_id;w.row_category_indices[i]=d->items[i].category_index;
        const uim_SettingsItemDesc *it=&d->items[i]; uim_Value value;size_t row_index=0,j;
        uim_BindingDesc b; uim_BindingId binding;
        for(j=0;j<i;j++)if(w.row_category_indices[j]==it->category_index)row_index++;
        (void)snprintf(key,sizeof(key),"settings.row.%s",it->stable_key);
        if((rc=add_node(r,w.page_id,w.scroll_node,d->panel_component_id,key,(uim_Rect){0,(float)row_index*(d->row_height+d->row_gap),row_w,d->row_height},&w.row_nodes[i],NULL,0))!=uim_OK)goto fail;
        if(d->category_count && it->category_index!=0 && (rc=uim_node_set_visible(r,w.row_nodes[i],false))!=uim_OK)goto fail;
        (void)snprintf(key,sizeof(key),"settings.label.%s",it->stable_key);
        if((rc=add_node(r,w.page_id,w.row_nodes[i],it->label_component_id,key,(uim_Rect){0,0,row_w*0.48f,d->row_height},&w.label_nodes[i],it->label_config,it->label_config_size))!=uim_OK)goto fail;
        (void)snprintf(key,sizeof(key),"settings.control.%s",it->stable_key);
        if((rc=add_node(r,w.page_id,w.row_nodes[i],it->control_component_id,key,(uim_Rect){row_w*0.52f,0,row_w*0.48f,d->row_height},&w.control_nodes[i],it->control_config,it->control_config_size))!=uim_OK)goto fail;
        if((rc=uim_property_get(r,it->property_id,&value))!=uim_OK)goto fail;
        memset(&b,0,sizeof(b));b.node_id=w.control_nodes[i];b.property_id=it->property_id;b.value_type=value.type;
        b.direction=it->direction;b.initial_sync=it->initial_sync;b.state=uim_BINDING_ACTIVE;
        if((rc=uim_binding_create(r,&b,&binding))!=uim_OK)goto fail;
        (void)binding;
    }
    *out=w;return uim_OK;
fail:
    if(w.page_id)(void)uim_page_destroy(r,w.page_id);
    uim_settings_window_dispose(&w);return rc;
}

uim_Result uim_settings_window_activate(UIM_ComponentManager *r,
                                        const uim_SettingsWindow *w,
                                        uim_NodeId node,
                                        uint64_t *out_serial) {
    size_t i,count=0; uim_PropertyId *ids=NULL; uim_Value *values=NULL; uim_SettingState st;
    uim_Result rc;
    uim_NodeInfo action_info;
    if(!r||!w||!w->page_id||!node)return uim_ERR_ARGUMENT;
    if(uim_node_get(r,node,&action_info)!=uim_OK||action_info.page_id!=w->page_id)return uim_ERR_NOT_FOUND;
    if(node==w->apply_button){if(!out_serial)return uim_ERR_ARGUMENT;return uim_apply_request_for(r,w->property_ids,w->item_count,out_serial);}
    if(node!=w->reset_button&&node!=w->cancel_button)return uim_ERR_NOT_FOUND;
    count=w->item_count;
    if(count){ids=(uim_PropertyId*)calloc(count,sizeof(*ids));values=(uim_Value*)calloc(count,sizeof(*values));if(!ids||!values){free(ids);free(values);return uim_ERR_NOMEM;}}
    for(i=0;i<count;i++){
        ids[i]=w->property_ids[i];
        if((rc=uim_setting_get_state(r,ids[i],&st))!=uim_OK)goto done;
        values[i]=(node==w->reset_button)?st.initial:st.committed;
    }
    rc=uim_setting_set_pending_batch(r,ids,values,count);
done:
    free(ids);free(values);return rc;
}

uim_Result uim_settings_window_select_category(UIM_ComponentManager*r,uim_SettingsWindow*w,uint32_t index){size_t i;uim_NodeInfo n;uim_Result rc;if(!r||!w||!w->page_id||!w->category_count||index>=w->category_count)return uim_ERR_ARGUMENT;for(i=0;i<w->category_count;i++)if(uim_node_get(r,w->category_nodes[i],&n)!=uim_OK||n.page_id!=w->page_id)return uim_ERR_STALE;for(i=0;i<w->item_count;i++){rc=uim_node_set_visible(r,w->row_nodes[i],w->row_category_indices[i]==index);if(rc!=uim_OK)return rc;}w->active_category=index;return uim_OK;}
uim_Result uim_settings_window_select_category_node(UIM_ComponentManager*r,uim_SettingsWindow*w,uim_NodeId node,uint32_t*out){size_t i;if(!r||!w||!node||!out)return uim_ERR_ARGUMENT;for(i=0;i<w->category_count;i++)if(w->category_nodes[i]==node){uim_Result rc=uim_settings_window_select_category(r,w,(uint32_t)i);if(rc==uim_OK)*out=(uint32_t)i;return rc;}return uim_ERR_NOT_FOUND;}

uim_Result uim_settings_window_dispatch_event(UIM_ComponentManager *r,
                                              const uim_SettingsWindow *w,
                                              const uim_Event *event,
                                              uint64_t *out_serial) {
    uim_NodeId expected;
    if(!r||!w||!event||!w->page_id)return uim_ERR_ARGUMENT;
    if(event->type!=uim_EVENT_ACTION)return uim_ERR_TYPE;
    switch(event->action_id){
    case uim_SETTINGS_RESET: expected=w->reset_button; break;
    case uim_SETTINGS_CANCEL: expected=w->cancel_button; break;
    case uim_SETTINGS_APPLY: expected=w->apply_button; break;
    default: return uim_ERR_UNSUPPORTED;
    }
    if(event->node_id!=expected)return uim_ERR_STATE;
    if(event->action_id==uim_SETTINGS_APPLY && !out_serial)return uim_ERR_ARGUMENT;
    if(out_serial)*out_serial=0;
    return uim_settings_window_activate(r,w,expected,out_serial);
}
