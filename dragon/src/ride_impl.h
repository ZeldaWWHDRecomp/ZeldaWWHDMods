#include "ride.h"
#include "quest_game.h"
#include "flight_math.h"
#include "quest_state.h"
#include "wwhd/camera.h"
#define BUTTON_A 0x8000u
#define BUTTON_B 0x4000u
#define DRAGON_TAG 0x4452474eu
#define ROPE_LENGTH 350.0f
dragon_ride_state dragon_ride={.id=~0u,.hand_offset={0,140,0},.lift_target=4000};
int dragon_valoo_resources;
static dragon_song_state song;
static int song_pending,summon_pending,boat_pending;
int dragon_tagged(void* actor) {return actor && dragon_actor(actor)->mParameters==DRAGON_TAG;}
int dragon_carried(void) {return dragon_ride.phase==DRAGON_LAUNCH || dragon_ride.phase==DRAGON_TAKEOFF || dragon_ride.phase==DRAGON_RIDING;}
static u32* play_word(u32 offset) {return dragon_word(wwhd_play_get(),offset);}
static cXyz left_hand(daPy_lk_c* player) {return *(cXyz*)((u8*)player+WWHD_OFFSET_Link_left_hand_position);}
static int hold_proc(daPy_lk_c* player) {
    if(dragon_ride.phase==DRAGON_SONG && player==dragon_ride.player) {
        if(dragon_ride.song_boat)*play_word(WWHD_PLAY_PLAYER_STATUS0_OFFSET)|=0x10000;
        wwhd_daPy_lk_c__setShipRidePosUseItem_023E2DC4(player);
    }
    return 1;
}
static int owns_proc(daPy_lk_c* p) {return p->mCurProcFunc.i==-1 && p->mCurProcFunc.f==(u32)&hold_proc;}
static void set_hold(daPy_lk_c* p) {p->mCurProcFunc.d=0;p->mCurProcFunc.i=-1;p->mCurProcFunc.f=(u32)&hold_proc;}
void dragon_place_link(daPy_lk_c* player) {
    dragon_ride_state* r=&dragon_ride;fopAc_ac_c* a=dragon_actor(player);
    cXyz pos=r->phase==DRAGON_LAUNCH?r->launch_link:(cXyz){r->pos.x+r->grab_offset.x-r->hand_offset.x,
        r->pos.y+r->grab_offset.y-r->hand_offset.y-ROPE_LENGTH,r->pos.z+r->grab_offset.z-r->hand_offset.z};
    a->current.pos=a->old.pos=pos;
    WWHD_GAME_DATA(WWHD_ADDR_Link_debug_position,cXyz)=pos;
    a->speed=(cXyz){0,0,0};a->speedF=a->gravity=0;
    s16 yaw=(s16)(r->yaw*32768/DRAGON_PI);
    a->current.angle.y=a->shape_angle.y=player->m34DE=yaw;
    WWHD_GAME_DATA(WWHD_ADDR_Link_debug_current_angle,csXyz).y=yaw;
    WWHD_GAME_DATA(WWHD_ADDR_Link_debug_shape_angle,csXyz).y=yaw;
}
void dragon_release(daPy_lk_c* player) {
    dragon_ride_state* r=&dragon_ride;
    if(dragon_carried() && player && (u32)player==*play_word(WWHD_PLAY_PLAYER_OFFSET)) {
        dragon_place_link(player);wwhd_fopAcM_seStartCurrent_0206A590(player,0x2821,0);
        *play_word(WWHD_PLAY_PLAYER_STATUS1_OFFSET)=(*play_word(WWHD_PLAY_PLAYER_STATUS1_OFFSET)&~0x20u)|r->old_leaf;
        wwhd_daPy_lk_c__commonProcInit_023DFDD8(player,0x27);
        wwhd_daPy_lk_c__procFall_init_023F6564(player,2,6);
        dragon_actor(player)->speed.y=8;dragon_actor(player)->speedF=12;
    }
    r->phase=r->actor || r->id!=~0u?DRAGON_LEAVING:DRAGON_IDLE;r->ticks=0;
}
static void summon(daPy_lk_c* player) {
    dragon_ride_state* r=&dragon_ride;
    if(r->phase!=DRAGON_IDLE || !dragon_sea() || dragon_event())return;
    int profile=dragon_profile((u32)&wwhd_daDr_Create_0212A830);if(profile<0){wwhd_log("[dragon] Valoo profile unresolved");return;}
    r->player=player;r->pos=dragon_actor(player)->current.pos;r->pos.y+=1700;
    r->yaw=dragon_actor(player)->shape_angle.y*(DRAGON_PI/32768);
    r->pos.x-=dragon_sin(r->yaw)*1800;r->pos.z-=dragon_cos(r->yaw)*1800;
    csXyz rotation={0,dragon_actor(player)->shape_angle.y,0};cXyz scale={.12f,.12f,.12f};
    r->id=wwhd_fopAcM_create_025D5834((u32)profile,DRAGON_TAG,(u32)&r->pos,-1,(u32)&rotation,(u32)&scale,0xff,0);
    if(r->id==~0u){wwhd_log("[dragon] summon failed");return;}
    r->phase=DRAGON_LOADING;r->ticks=0;
}
static void launch(daPy_lk_c* player) {
    dragon_ride_state* r=&dragon_ride;
    r->launch_link=dragon_actor(player)->current.pos;r->old_leaf=*play_word(WWHD_PLAY_PLAYER_STATUS1_OFFSET)&0x20;
    wwhd_daPy_lk_c__procFall_init_023F6564(player,2,0);set_hold(player);
    wwhd_daPy_lk_c__setSingleMoveAnime_023E0A04(player,8,0,0,-1,3);
    wwhd_daPy_lk_c__setActAnimeUpper_023DE7E8(player,0xe4,2,0,0,-1,-1);
    r->phase=DRAGON_LAUNCH;r->ticks=0;
}
static void pickup(daPy_lk_c* player) {
    dragon_ride_state* r=&dragon_ride;
    wwhd_fopAcM_seStartCurrent_0206A590(r->actor,0x4841,0);
    wwhd_fopAcM_seStartCurrent_0206A590(player,0x2830,0);
    wwhd_daPy_lk_c__setSingleMoveAnime_023E0A04(player,0x78,.5f,0,-1,4);
    r->lift_start=r->pos.y;float water=player->mWaterY;
    if(!dragon_finite(water)||dragon_abs(water)>=10000)water=0;
    r->lift_target=r->pos.y>water+4000?r->pos.y:water+4000;r->phase=DRAGON_TAKEOFF;r->ticks=0;
}
typedef struct {float minimum,maximum,water;} height_bounds;
static height_bounds bounds(daPy_lk_c* p) {
    dragon_ride_state* r=&dragon_ride;
    float ground=*(float*)((u8*)p+WWHD_OFFSET_Link_ground_height),water=p->mWaterY;
    float level=dragon_finite(water)&&dragon_abs(water)<10000?water:0;
    float clearance=r->hand_offset.y+ROPE_LENGTH-r->grab_offset.y+800;
    if(clearance<800)clearance=800;
    float floor=level+clearance,ceiling=level+16000;
    if(dragon_finite(ground)&&ground>-1e8f && ground+(clearance>470?clearance:470)>floor)floor=ground+(clearance>470?clearance:470);
    if(dragon_finite(water)&&water>-1e8f && water+clearance>floor)floor=water+clearance;
    return (height_bounds){floor<ceiling?floor:ceiling,ceiling,level};
}
static int blocked(cXyz from,cXyz to) {
    dragon_ride_state* r=&dragon_ride;wwhd_line_check_storage storage;
    wwhd_cBgS_LinChk_ctor_02008FEC(&storage);
    for(unsigned i=0;i<7;++i)storage.bytes[WWHD_OFFSET_line_check_pass_flags+i]=i==0;
    *dragon_word(&storage,WWHD_OFFSET_line_check_group)=1;
    *dragon_word(&storage,WWHD_OFFSET_line_check_pass_pointer0)=(u32)storage.bytes+WWHD_OFFSET_line_check_vtable_58;
    *dragon_word(&storage,WWHD_OFFSET_line_check_pass_pointer1)=(u32)storage.bytes+WWHD_OFFSET_line_check_vtable_64;
    *dragon_word(&storage,WWHD_OFFSET_line_check_vtable_10)=(u32)&WWHD_GAME_DATA(WWHD_ADDR_ObjHole_line_check_vtable_10,u8);
    *dragon_word(&storage,WWHD_OFFSET_line_check_vtable_20)=(u32)&WWHD_GAME_DATA(WWHD_ADDR_ObjHole_line_check_vtable_20,u8);
    *dragon_word(&storage,WWHD_OFFSET_line_check_vtable_58)=(u32)&WWHD_GAME_DATA(WWHD_ADDR_ObjHole_line_check_vtable_58,u8);
    *dragon_word(&storage,WWHD_OFFSET_line_check_vtable_64)=(u32)&WWHD_GAME_DATA(WWHD_ADDR_ObjHole_line_check_vtable_64,u8);
    cXyz offsets[8]={{0,0,0},{0,500,0},{0,-250,0},{600,0,0},{-600,0,0},{0,0,600},{0,0,-600},
        {r->grab_offset.x,r->grab_offset.y-r->hand_offset.y-ROPE_LENGTH,r->grab_offset.z}};
    float dx=to.x-from.x,dy=to.y-from.y,dz=to.z-from.z,length=dragon_sqrt(dx*dx+dy*dy+dz*dz);
    if(length<.01f)return 0;
    float ahead=200/length;
    for(unsigned i=0;i<8;++i) {
        cXyz start={from.x+offsets[i].x,from.y+offsets[i].y,from.z+offsets[i].z};
        cXyz end={to.x+offsets[i].x+dx*ahead,to.y+offsets[i].y+dy*ahead,to.z+offsets[i].z+dz*ahead};
        wwhd_dBgS_LinChk_Set_024F1AFC((u32)&storage,(u32)&start,(u32)&end,(u32)r->actor);
        if(wwhd_line_cross_02008860(wwhd_play_get()+WWHD_PLAY_BG_COLLISION_OFFSET,&storage))return 1;
    }
    return 0;
}
static void move_checked(cXyz previous) {
    dragon_ride_state* r=&dragon_ride;
    if(blocked(previous,r->pos)) {
        cXyz vertical={previous.x,r->pos.y,previous.z};r->pos=blocked(previous,vertical)?previous:vertical;++r->blocked_ticks;
    } else r->blocked_ticks=0;
}
static void fly(daPy_lk_c* player,wwhd_input_state pad) {
    dragon_ride_state* r=&dragon_ride;r->yaw=dragon_wrap(r->yaw+pad.lx*.035f*dragon_tick_delta());
    r->bank+=(-pad.lx*.20f-r->bank)*dragon_blend(.10f,dragon_tick_delta());r->pitch+=(pad.ly*.65f-r->pitch)*dragon_blend(.08f,dragon_tick_delta());
    r->pos.x=dragon_clamp(r->pos.x+dragon_sin(r->yaw)*140*dragon_tick_delta(),-345000,345000);
    r->pos.z=dragon_clamp(r->pos.z+dragon_cos(r->yaw)*140*dragon_tick_delta(),-345000,345000);
    height_bounds h=bounds(player);r->pos.y=dragon_clamp(r->pos.y+dragon_sin(r->pitch)*160*dragon_tick_delta(),h.minimum,h.maximum);
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_mDoAud_tact_judge,s32,original_song_judge,(s32 index,s32 direction));
WWHD_REPLACE(WWHD_ADDR_mDoAud_tact_judge,s32,dragon_song_judge,(s32 index,s32 direction)) {
    s32 result=original_song_judge(index,direction);
    daPy_lk_c* p=(daPy_lk_c*)*play_word(WWHD_PLAY_PLAYER_OFFSET);
    int eligible=p && dragon_quest_song_allowed(p) && dragon_ride.phase==DRAGON_IDLE && dragon_sea() && p->mCurProc==0x9a && p->mProcVar6==-1;
    if(dragon_song_beat(&song,(unsigned)index,(unsigned)direction,wwhd_tact_get_beat(),eligible)) {song_pending=1;return -1;}
    return result;
}
static void end_song(daPy_lk_c* player) {
    dragon_ride_state* r=&dragon_ride;
    if(r->song_wind){wwhd_daPy_callback_end_023D4538(&player->m33E8);r->song_wind=0;}
    wwhd_daPy_callback_end_023D4538(&player->m32E4);
    wwhd_play_get()[WWHD_PLAY_METRONOME_OFFSET]=0;
    u16* flags=(u16*)(wwhd_play_get()+WWHD_PLAY_EVENT_FLAGS_OFFSET);*flags|=8;
    void* cam=wwhd_dCam_getBody_024F8044();wwhd_dCamera_c_EndEventCamera_0253E860((u32)cam,*dragon_word(player,WWHD_OFFSET_actor_process_id));
    if(r->song_boat && dragon_sea()){*play_word(WWHD_PLAY_PLAYER_STATUS0_OFFSET)|=0x10000;wwhd_daPy_lk_c__setShipRidePosUseItem_023E2DC4(player);}
    wwhd_daPy_lk_c__endDemoMode_023F2048(player);wwhd_tact_reset();
    boat_pending=r->song_boat && dragon_sea();r->song_boat=0;
    *play_word(WWHD_PLAY_PLAYER_STATUS1_OFFSET)&=~1u;r->phase=DRAGON_IDLE;song.matched=0;
}
static void begin_song(daPy_lk_c* player) {
    dragon_ride_state* r=&dragon_ride;r->player=player;r->song_boat=wwhd_daPy_lk_c__checkShipRideUseItem_023E26EC(player,0);
    wwhd_seStart_025E1988(0x896);wwhd_daPy_lk_c__procTactPlay_init_02439D54(player,0,0,1);
    u32 model=*dragon_word(player,WWHD_OFFSET_Link_equipped_item_model);
    if(model && !player->m33E8.mpEmitter) {
        wwhd_daPy_makeEmitter_023D457C(&player->m33E8,0x32,(u8*)model+WWHD_OFFSET_J3DModel_base_matrix,&dragon_actor(player)->current.pos,0);
        r->song_wind=player->m33E8.mpEmitter!=0;
    }
    player->mCurProc=0x9a;player->mProcVar6=player->mProcVar7=-1;set_hold(player);r->phase=DRAGON_SONG;r->ticks=0;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__execute,s32,original_link_execute,(void* self));
WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__execute,s32,dragon_execute,(void* self)) {
    daPy_lk_c* p=self;dragon_ride_state* r=&dragon_ride;float delta=dragon_tick_delta();wwhd_input_state pad;wwhd_input_read(&pad);
    u32 pressed=pad.buttons&~r->buttons;r->buttons=pad.buttons;
    dragon_hud_register();
    if(r->phase==DRAGON_SONG && ((pressed&BUTTON_B)||!dragon_sea()||p!=r->player||!owns_proc(p))) {
        if(p==r->player)end_song(p);else {r->phase=DRAGON_IDLE;song.matched=0;r->song_wind=0;}
        song_pending=summon_pending=0;
    }
    if(dragon_carried() && (p!=r->player||!dragon_sea()||dragon_event()||!dragon_tagged(r->actor)))dragon_release(r->player);
    if(r->phase==DRAGON_RIDING) {
        if(pressed&BUTTON_A)dragon_release(p);
        else {cXyz previous=r->pos;fly(p,pad);move_checked(previous);dragon_place_link(p);*play_word(WWHD_PLAY_PLAYER_STATUS1_OFFSET)|=0x20;}
    }
    if(r->phase==DRAGON_TAKEOFF || r->phase==DRAGON_LAUNCH) {
        if(pressed&BUTTON_A)dragon_release(p);else {dragon_place_link(p);*play_word(WWHD_PLAY_PLAYER_STATUS1_OFFSET)|=0x20;}
    }
    int quiet=dragon_quest_tick(p,pressed,r->phase==DRAGON_IDLE);
    /* Suppress only fields proven by public Link/controller uses. Unknown
     * prototype trailing words are deliberately not treated as input fields. */
    u32 controller=WWHD_GAME_DATA(WWHD_ADDR_pad_pointer,u32),saved[6];
    const u32 offsets[6]={WWHD_OFFSET_pad_trigger,WWHD_OFFSET_pad_buttons_124,WWHD_OFFSET_pad_axis_130,WWHD_OFFSET_pad_axis_134,WWHD_OFFSET_pad_axis_138,WWHD_OFFSET_pad_axis_13C};
    if(quiet && controller)for(unsigned i=0;i<6;++i){saved[i]=*(u32*)(controller+offsets[i]);*(u32*)(controller+offsets[i])=0;}
    s32 result=original_link_execute(self);
    if(quiet && controller)for(unsigned i=0;i<6;++i)*(u32*)(controller+offsets[i])=saved[i];
    if((boat_pending||summon_pending) && (p!=r->player||!dragon_sea()))boat_pending=summon_pending=0;
    if(r->phase==DRAGON_IDLE && !dragon_event()) {
        if(boat_pending){boat_pending=0;wwhd_daPy_lk_c__procShipPaddle_init_023F1E60(p);}
        if(summon_pending){summon_pending=0;summon(p);}
    }
    if(song_pending && r->phase==DRAGON_IDLE){song_pending=0;begin_song(p);}
    else if(r->phase==DRAGON_SONG && (r->ticks+=delta)>=90){dragon_quest_song_learned();end_song(p);summon_pending=1;}
    if(r->phase==DRAGON_LOADING && (r->ticks+=delta)>300)dragon_release(p);
    if(r->phase==DRAGON_APPROACH && p==r->player && r->grab_valid) {
        cXyz target=dragon_actor(p)->current.pos;target.x-=r->grab_offset.x;target.y+=r->hand_offset.y+ROPE_LENGTH-r->grab_offset.y;target.z-=r->grab_offset.z;
        r->pos.x+=(target.x-r->pos.x)*dragon_blend(.08f,dragon_tick_delta());r->pos.y+=(target.y-r->pos.y)*dragon_blend(.08f,dragon_tick_delta());r->pos.z+=(target.z-r->pos.z)*dragon_blend(.08f,dragon_tick_delta());
        if(dragon_abs(target.x-r->pos.x)+dragon_abs(target.y-r->pos.y)+dragon_abs(target.z-r->pos.z)<45)launch(p);
    } else if(r->phase==DRAGON_LAUNCH) {
        if(!owns_proc(p))dragon_release(p);else {
            float previous_ticks=r->ticks;r->ticks+=delta;
            if(previous_ticks<8 && r->ticks>=8){wwhd_daPy_lk_c__setActAnimeUpper_023DE7E8(p,0xe2,2,1.3f,1,0xb,-1);wwhd_fopAcM_seStartCurrent_0206A590(p,0x2817,0);}
            cXyz hand=left_hand(p),claw={r->pos.x+r->grab_offset.x,r->pos.y+r->grab_offset.y,r->pos.z+r->grab_offset.z};
            float t=dragon_clamp(((float)r->ticks-8)/18,0,1);
            r->hook_pos=(cXyz){hand.x+(claw.x-hand.x)*t,hand.y+(claw.y-hand.y)*t,hand.z+(claw.z-hand.z)*t};
            if(r->ticks>=32)pickup(p);
        }
    } else if(r->phase==DRAGON_TAKEOFF) {
        cXyz previous=r->pos;float t=dragon_clamp((r->ticks+=delta)/60.0f,0,1),eased=t*t*(3-2*t);
        r->pos.y=r->lift_start+(r->lift_target-r->lift_start)*eased;r->pos.x+=dragon_sin(r->yaw)*60*delta;r->pos.z+=dragon_cos(r->yaw)*60*delta;
        r->pitch+=(.28f-r->pitch)*dragon_blend(.12f,delta);move_checked(previous);dragon_place_link(p);
        if(r->ticks>=60){r->phase=DRAGON_RIDING;r->ticks=0;}
    }
    if(r->phase==DRAGON_TAKEOFF || r->phase==DRAGON_RIDING) {
        cXyz root=dragon_actor(p)->current.pos,hand=left_hand(p),offset={hand.x-root.x,hand.y-root.y,hand.z-root.z};
        if(dragon_finite(offset.y) && dragon_abs(offset.x)+dragon_abs(offset.y)+dragon_abs(offset.z)<500)r->hand_offset=offset;
    }
    if(r->phase==DRAGON_TAKEOFF && !owns_proc(p))dragon_release(p);
    if(r->phase==DRAGON_RIDING) {
        if(!owns_proc(p))dragon_release(p);else {height_bounds h=bounds(p);r->pos.y=dragon_clamp(r->pos.y,h.minimum,h.maximum);dragon_place_link(p);r->ticks+=delta;}
    }
    height_bounds h=bounds(p);dragon_hud.visible=dragon_sea();dragon_hud.phase=r->phase;
    dragon_hud.altitude=r->pos.y-h.water;dragon_hud.minimum=h.minimum-h.water;dragon_hud.notes=song.matched;dragon_hud.conducting=p->mCurProc==0x9a;
    return result;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daPy_lk_c__posMove,void,original_position,(void* self));
WWHD_REPLACE(WWHD_ADDR_daPy_lk_c__posMove,void,dragon_position,(void* self)) {
    if(dragon_carried() && self==dragon_ride.player)dragon_place_link(self);else original_position(self);
}
WWHD_HOOK_RETURN(WWHD_ADDR_dCamera_c__followCamera,dragon_camera,(dCamera_c* camera)) {
    if(!dragon_carried())return;
    dragon_ride_state* r=&dragon_ride;
    cXyz center={r->pos.x,r->pos.y+120,r->pos.z};
    if(r->phase==DRAGON_LAUNCH)center=(cXyz){r->launch_link.x,r->launch_link.y+300,r->launch_link.z};
    cXyz eye={r->pos.x-dragon_sin(r->yaw)*1800,r->pos.y+750,r->pos.z-dragon_cos(r->yaw)*1800};
    eye=(cXyz){camera->mEye.x+(eye.x-camera->mEye.x)*dragon_blend(.18f,dragon_tick_delta()),camera->mEye.y+(eye.y-camera->mEye.y)*dragon_blend(.18f,dragon_tick_delta()),camera->mEye.z+(eye.z-camera->mEye.z)*dragon_blend(.18f,dragon_tick_delta())};
    wwhd_dCamera_Reset4_025151BC(camera,&center,&eye,60,0);
}
