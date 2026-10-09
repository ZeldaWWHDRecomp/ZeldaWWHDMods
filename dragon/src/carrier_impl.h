#include "ride.h"
#include "flight_math.h"
#include "wwhd/valoo.h"
static const char valoo_archive[]="Demo45",link_archive[]="Link";
WWHD_GAME_ORIGINAL(WWHD_ADDR_fopAcM_entrySolidHeap,s32,original_solid_heap,(u32 self,u32 callback,u32 size));
WWHD_REPLACE(WWHD_ADDR_fopAcM_entrySolidHeap,s32,dragon_solid_heap,(u32 self,u32 callback,u32 size)) {
    return original_solid_heap(self,callback,dragon_tagged((void*)self)?0x18000:size);
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_createHeap_0212A728,s32,original_actor_heap,(void* self));
WWHD_REPLACE(WWHD_ADDR_createHeap_0212A728,s32,dragon_actor_heap,(void* self)) {
    s32 result=original_actor_heap(self);if(!result || !dragon_tagged(self))return result;
    int resources=dragon_valoo_resources;dragon_valoo_resources=0;
    wwhd_safe_string key={(u32)link_archive,(u32)&WWHD_GAME_DATA(WWHD_ADDR_Himo2_SafeString_vtable,u8)};
    u32 data=wwhd_Lookup_026066C4_026066C4(WWHD_GAME_DATA(WWHD_ADDR_dComIfG_resControl_pointer,u32),(u32)&key,0x2e);
    /* Public ext_modelCreate reads only data and modelFlag. Its body never reads
     * the legacy extra dlFlag register; the original caller's modelFlag is 0. */
    dragon_ride.hook_model=data?wwhd_ext_modelCreate_025E38E0((void*)data,0):0;
    if(dragon_ride.hook_model)*(cXyz*)(dragon_ride.hook_model+WWHD_OFFSET_J3DModel_base_scale)=(cXyz){1,1,1};
    dragon_valoo_resources=resources;return dragon_ride.hook_model?result:0;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daDr_Create,s32,original_actor_create,(void* self));
WWHD_REPLACE(WWHD_ADDR_daDr_Create,s32,dragon_actor_create,(void* self)) {
    if(!dragon_tagged(self))return original_actor_create(self);
    int resources=dragon_valoo_resources;dragon_valoo_resources=1;
    s32 result=original_actor_create(self);dragon_valoo_resources=resources;
    dragon_ride_state* r=&dragon_ride;fopAc_ac_c* a=dragon_actor(self);
    if(result==4) {
        r->actor=a;r->audio_pos=r->pos;r->rope=(u32)wwhd_ext_lineMat0Ctor_025E9960(0);
        if(r->rope && !wwhd_ext_lineMat0Init_025E9B80((void*)r->rope,1,8,0)) {
            wwhd_ext_lineMat0Dtor_025E99E0((void*)r->rope,3);r->rope=0;
        }
        if(r->phase==DRAGON_LOADING)r->phase=DRAGON_APPROACH;
        wwhd_anm_init_02129DB4(a,0x10,5,2,1,-1);
        a->cullSizeFar=100000;a->actor_status=(a->actor_status&~0x100u)|0x80;
    } else if(result==5){r->phase=DRAGON_IDLE;r->id=~0u;}
    return result;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daDr_Execute,s32,original_actor_execute,(void* self));
WWHD_REPLACE(WWHD_ADDR_daDr_Execute,s32,dragon_actor_execute,(void* self)) {
    if(!dragon_tagged(self))return original_actor_execute(self);
    dragon_ride_state* r=&dragon_ride;fopAc_ac_c* a=dragon_actor(self);
    if(!dragon_sea()){wwhd_fopAcM_delete_actor_025D57E0((u32)a);return 1;}
    if(r->phase==DRAGON_LEAVING) {
        float delta=dragon_tick_delta();r->pos.y+=35*delta;r->pos.z+=80*delta;
        if((r->ticks+=delta)>90){wwhd_fopAcM_delete_actor_025D57E0((u32)a);return 1;}
    }
    a->old.pos=a->current.pos=a->eyePos=r->pos;a->scale=(cXyz){.12f,.12f,.12f};
    a->current.angle.x=a->shape_angle.x=(s16)(-r->pitch*32768/DRAGON_PI);
    a->current.angle.y=a->shape_angle.y=(s16)(r->yaw*32768/DRAGON_PI);
    a->current.angle.z=a->shape_angle.z=(s16)(r->bank*32768/DRAGON_PI);
    float dx=r->pos.x-r->audio_pos.x,dy=r->pos.y-r->audio_pos.y,dz=r->pos.z-r->audio_pos.z;
    u32 intensity=(u32)dragon_clamp(dragon_sqrt(dx*dx+dy*dy+dz*dz)*100/(160*dragon_tick_delta()),0,100);
    wwhd_fopAcM_seStartCurrent_0206A590(a,0x5125,intensity);
    if(r->phase==DRAGON_TAKEOFF || r->phase==DRAGON_RIDING)wwhd_fopAcM_seStartCurrent_0206A590(a,0x5076,intensity);
    r->audio_pos=r->pos;
    mDoExt_McaMorf* morf=(mDoExt_McaMorf*)((dr_class*)self)->mpMorf;
    if(!morf)return 1;
    wwhd_ext_play_025E535C(morf,&a->current.pos,0,0);wwhd_daDr_setMtx_02129F0C(a);
    u32 block=morf->mpModel?*(u32*)(morf->mpModel+WWHD_OFFSET_J3DModel_mpMtxBlock):0;
    u32 matrices=block?*(u32*)(block+WWHD_OFFSET_J3DMtxBlock_mpMtx):0;
    if(matrices>=0x10000000 && matrices<0x70000000) {
        /* Existing Demo45 claw attachment: original matrix index 3. Mtx34 is
         * twelve floats; its translation column is component 3 of each row. */
        float* matrix=(float*)matrices+3*12;
        cXyz delta={matrix[3]-r->pos.x,matrix[7]-r->pos.y,matrix[11]-r->pos.z};
        if(dragon_finite(delta.x)&&dragon_finite(delta.y)&&dragon_finite(delta.z) &&
           dragon_abs(delta.x)+dragon_abs(delta.y)+dragon_abs(delta.z)<10000){r->grab_offset=delta;r->grab_valid=1;}
    }
    return 1;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daDr_Draw,s32,original_actor_draw,(void* self));
WWHD_REPLACE(WWHD_ADDR_daDr_Draw,s32,dragon_actor_draw,(void* self)) {
    s32 result=original_actor_draw(self);dragon_ride_state* r=&dragon_ride;
    if(!dragon_tagged(self)||!r->rope||!dragon_carried()||!r->player)return result;
    if(r->phase==DRAGON_LAUNCH && r->ticks<8)return result;
    u32 table=*(u32*)(r->rope+WWHD_OFFSET_line_mat0_points_table);if(!table)return result;
    cXyz* points=*(cXyz**)table;if(!points)return result;
    cXyz claw={r->pos.x+r->grab_offset.x,r->pos.y+r->grab_offset.y,r->pos.z+r->grab_offset.z};
    cXyz hands=dragon_actor(r->player)->current.pos;
    if(r->phase==DRAGON_LAUNCH){hands=*(cXyz*)((u8*)r->player+WWHD_OFFSET_Link_left_hand_position);claw=r->hook_pos;}
    else hands=(cXyz){hands.x+r->hand_offset.x,hands.y+r->hand_offset.y,hands.z+r->hand_offset.z};
    if(r->hook_model) {
        cXyz z={claw.x-hands.x,claw.y-hands.y,claw.z-hands.z};float length=dragon_sqrt(z.x*z.x+z.y*z.y+z.z*z.z);
        z=length>.01f?(cXyz){z.x/length,z.y/length,z.z/length}:(cXyz){0,1,0};
        cXyz x={dragon_cos(r->yaw)*z.y,z.z*dragon_sin(r->yaw)-z.x*dragon_cos(r->yaw),-dragon_sin(r->yaw)*z.y};
        float width=dragon_sqrt(x.x*x.x+x.y*x.y+x.z*x.z);
        x=width>.01f?(cXyz){x.x/width,x.y/width,x.z/width}:(cXyz){1,0,0};
        cXyz y={z.y*x.z-z.z*x.y,z.z*x.x-z.x*x.z,z.x*x.y-z.y*x.x};
        float matrix[12]={x.x*.2f,y.x*.2f,z.x*.2f,claw.x-z.x*3,x.y*.2f,y.y*.2f,z.y*.2f,claw.y-z.y*3,x.z*.2f,y.z*.2f,z.z*.2f,claw.z-z.z*3};
        memcpy((u8*)r->hook_model+WWHD_OFFSET_J3DModel_base_matrix,matrix,sizeof matrix);
        wwhd_dScnKy_env_light_c_setLightTevColorType_02562F5C(wwhd_dKy_getEnvlight_02555D0C(),r->hook_model,(u32)&dragon_actor(self)->tevStr);
        wwhd_ext_2DE0_025E2DE0((void*)r->hook_model,0);
    }
    for(unsigned i=0;i<8;++i) {
        float t=i/7.0f;points[i]=(cXyz){claw.x+(hands.x-claw.x)*t,claw.y+(hands.y-claw.y)*t,claw.z+(hands.z-claw.z)*t};
    }
    u32 color=0xb99158ff;
    wwhd_ext_lineUpdate_025EA548((void*)r->rope,8,&color,0,&dragon_actor(self)->tevStr,5);
    u32 vtable=*(u32*)(r->rope+WWHD_OFFSET_line_material_vtable);
    u32 (*material_id)(void*)=(u32(*)(void*))*(u32*)(vtable+WWHD_OFFSET_line_material_id_vslot);
    u32 id=material_id((void*)r->rope);
    if(id<8)wwhd_ext_sortSet_025EDD04(wwhd_play_get()+WWHD_PLAY_LINE_PACKETS_OFFSET+id*WWHD_STRIDE_line_packet,(void*)r->rope);
    return result;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daDr_Delete,s32,original_actor_delete,(void* self));
WWHD_REPLACE(WWHD_ADDR_daDr_Delete,s32,dragon_actor_delete,(void* self)) {
    dragon_ride_state* r=&dragon_ride;
    if(self==r->actor) {
        if(dragon_carried())dragon_release(r->player);
        if(r->rope)wwhd_ext_lineMat0Dtor_025E99E0((void*)r->rope,3);
        u32 buttons=r->buttons;*r=(dragon_ride_state){.id=~0u,.buttons=buttons,.hand_offset={0,140,0},.lift_target=4000};
    }
    int resources=dragon_valoo_resources;if(dragon_tagged(self))dragon_valoo_resources=1;
    s32 result=original_actor_delete(self);dragon_valoo_resources=resources;return result;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_dComIfG_resLoad,s32,original_resource_load,(u32 phase,u32 name));
WWHD_REPLACE(WWHD_ADDR_dComIfG_resLoad,s32,dragon_resource_load,(u32 phase,u32 name)) {
    return original_resource_load(phase,dragon_valoo_resources?(u32)valoo_archive:name);
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_dComIfG_resDelete,s32,original_resource_delete,(u32 phase,u32 name));
WWHD_REPLACE(WWHD_ADDR_dComIfG_resDelete,s32,dragon_resource_delete,(u32 phase,u32 name)) {
    return original_resource_delete(phase,dragon_valoo_resources?(u32)valoo_archive:name);
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_Lookup_026066C4,u32,original_resource_get,(u32 self,u32 name,u32 index));
WWHD_REPLACE(WWHD_ADDR_Lookup_026066C4,u32,dragon_resource_get,(u32 self,u32 name,u32 index)) {
    if(!dragon_valoo_resources)return original_resource_get(self,name,index);
    if(index==8)return 0; /* Native Dr BAS does not belong to the flight archive. */
    wwhd_safe_string* key=(wwhd_safe_string*)name;u32 previous=key->mStringTop;key->mStringTop=(u32)valoo_archive;
    u32 result=original_resource_get(self,name,index==0x13?43:16);key->mStringTop=previous;return result;
}
