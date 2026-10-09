/* Extra actors carry private parameter tags; normal story actors call originals. */
#include "quest.h"
#include "quest_game.h"
#include "hud.h"
#include "flight_math.h"
#include "wwhd/medli.h"
#define MEDLI_TAG 0x44510000u
#define BUTTON_A 0x8000u
#define BUTTON_B 0x4000u
#define BUTTON_LEFT 0x0800u
static const cXyz lookout={202100,2562,-199900};
static const cXyz chimes[3]={{197720,94,-199470},{201800,2562,-199900},{209430,1900,-202600}};
typedef struct {u32 id;fopAc_ac_c* actor;float hit;} quest_actor;
static quest_actor medli={.id=~0u},gongs[3]={{.id=~0u},{.id=~0u},{.id=~0u}};
static int creating_medli;
static int tagged_medli(void* actor) {return actor && dragon_actor(actor)->mParameters==MEDLI_TAG;}
static int tagged_gong(void* actor) {
    u32 parameter=actor?dragon_actor(actor)->mParameters:0;
    return parameter>MEDLI_TAG && parameter<=MEDLI_TAG+3?(int)(parameter-MEDLI_TAG-1):-1;
}
static void forget(quest_actor* actor) {*actor=(quest_actor){.id=~0u};}
static void spawn(quest_actor* actor,u32 create,u32 parameter,cXyz position) {
    if(actor->id!=~0u)return;
    int profile=dragon_profile(create);if(profile<0)return;
    csXyz rotation={0,0,0};cXyz scale={1,1,1};
    actor->id=wwhd_fopAcM_create_025D5834((u32)profile,parameter,(u32)&position,13,
        (u32)&rotation,(u32)&scale,0xff,0);
}
int dragon_quest_song_allowed(daPy_lk_c* player) {
    return dragon_quest.progress.phase==4 || (dragon_quest.progress.phase==3 &&
        !dragon_quest.dialog && dragon_roost(player) && medli.actor &&
        dragon_distance_squared(dragon_actor(player)->current.pos,lookout)<650*650);
}
void dragon_quest_song_learned(void) {
    if(dragon_quest_learn(&dragon_quest))dragon_quest_persist();
}
int dragon_quest_tick(daPy_lk_c* player,u32 pressed,int idle) {
    u32 save=WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer,u32);
    dragon_hud.quest_visible=0;
    if(!save || !dragon_sea()){dragon_quest.dialog=0;return 0;}
    int leaf=0;
    for(unsigned i=0;i<WWHD_COUNT_save_inventory;++i)
        leaf|=*(u8*)(save+WWHD_OFFSET_save_inventory+i)==0x34;
    if(dragon_quest_invite(&dragon_quest,leaf,wwhd_dComIfGs_isStageBossEnemy_02520A84(3)))dragon_quest_persist();
    float delta=dragon_tick_delta();
    if(dragon_quest.toast>0)dragon_quest.toast-=delta;
    if(dragon_quest.cooldown>0)dragon_quest.cooldown-=delta;
    if(dragon_quest.progress.phase>=1 && dragon_roost(player) && dragon_quest.cooldown<=0) {
        spawn(&medli,(u32)&wwhd_daNpc_Md_Create_02286B58,MEDLI_TAG,lookout);
        if(dragon_quest.progress.phase>=2)for(unsigned i=0;i<3;++i)
            spawn(&gongs[i],(u32)&wwhd_Mthd_Create_0234C308,MEDLI_TAG+1+i,chimes[i]);
        dragon_quest.cooldown=30;
    }
    int consumed=dragon_quest.dialog!=0;
    if(idle && !dragon_event()) {
        if(!dragon_quest.dialog && dragon_quest.progress.phase==1 && (pressed&BUTTON_LEFT))dragon_quest.dialog=1;
        if(!dragon_quest.dialog && dragon_roost(player) && medli.actor &&
           dragon_distance_squared(dragon_actor(player)->current.pos,lookout)<450*450 && (pressed&BUTTON_A))
            dragon_quest.dialog=(int)dragon_quest.progress.phase+1;
        if(dragon_quest.dialog && (pressed&BUTTON_B))dragon_quest.dialog=0;
        else if(consumed && (pressed&BUTTON_A)) {
            if(dragon_quest.dialog==2 && dragon_quest_accept(&dragon_quest))dragon_quest_persist();
            dragon_quest.dialog=0;
        }
        if(dragon_quest.progress.phase==2 && !dragon_quest.dialog &&
           player->mCurProc>=0x41 && player->mCurProc<=0x4a &&
           player->mFrameCtrlUnder[0].mFrame>=4 && player->mFrameCtrlUnder[0].mFrame<=15) {
            cXyz root=dragon_actor(player)->current.pos;
            float yaw=dragon_actor(player)->shape_angle.y*(DRAGON_PI/32768);
            for(unsigned i=0;i<3;++i) {
                float dx=chimes[i].x-root.x,dz=chimes[i].z-root.z;
                if(gongs[i].actor && dragon_abs(chimes[i].y-root.y)<220 && dx*dx+dz*dz<400*400 &&
                   dragon_sin(yaw)*dx+dragon_cos(yaw)*dz>0 && dragon_quest_chime(&dragon_quest,i)) {
                    gongs[i].hit=35;
                    mDoExt_McaMorf* morf=(mDoExt_McaMorf*)*dragon_word(gongs[i].actor,WWHD_OFFSET_daObjGong_Act_c_mpMorf);
                    if(morf)morf->mFrameCtrl.mFrame=0;
                    wwhd_fopAcM_seStartCurrent_0206A590(gongs[i].actor,0x69d3,0);
                    dragon_quest_persist();
                }
            }
        }
    }
    dragon_hud.quest_visible=idle && (dragon_quest.progress.phase<4 ||
        (dragon_roost(player) && medli.actor && dragon_distance_squared(dragon_actor(player)->current.pos,lookout)<450*450));
    return consumed || dragon_quest.dialog;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_dSv_player_collect_c_isCollect,s32,original_collect,(u32 self,u32 field,u32 number));
WWHD_REPLACE(WWHD_ADDR_dSv_player_collect_c_isCollect,s32,dragon_collect,(u32 self,u32 field,u32 number)) {
    return creating_medli && !field && number==2?0:original_collect(self,field,number);
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_Act_c___create_0234C0B0,s32,original_gong_create,(void* self));
WWHD_REPLACE(WWHD_ADDR_Act_c___create_0234C0B0,s32,dragon_gong_create,(void* self)) {
    int index=tagged_gong(self);s32 result=original_gong_create(self);
    if(index>=0) {
        if(result==4) {
            gongs[index].actor=dragon_actor(self);*dragon_word(self,WWHD_OFFSET_actor_attention_flags)=0;
            mDoExt_McaMorf* morf=(mDoExt_McaMorf*)*dragon_word(self,WWHD_OFFSET_daObjGong_Act_c_mpMorf);
            if(morf){morf->mFrameCtrl.mRate=0;morf->mFrameCtrl.mFrame=0;}
        } else if(result==5)forget(&gongs[index]);
    }
    return result;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_Act_c___execute_0234C23C,u8,original_gong_execute,(void* self));
WWHD_REPLACE(WWHD_ADDR_Act_c___execute_0234C23C,u8,dragon_gong_execute,(void* self)) {
    int index=tagged_gong(self);if(index<0)return original_gong_execute(self);
    mDoExt_McaMorf* morf=(mDoExt_McaMorf*)*dragon_word(self,WWHD_OFFSET_daObjGong_Act_c_mpMorf);
    if(morf){morf->mFrameCtrl.mRate=gongs[index].hit>0?1:0;wwhd_ext_play_025E535C(morf,0,0,0);}
    if(gongs[index].hit>0)gongs[index].hit-=dragon_tick_delta();
    wwhd_Act_c__set_mtx_0234BFC4(self);return 1;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_Act_c___delete_0234C1C4,u8,original_gong_delete,(void* self));
WWHD_REPLACE(WWHD_ADDR_Act_c___delete_0234C1C4,u8,dragon_gong_delete,(void* self)) {
    int index=tagged_gong(self);u8 result=original_gong_delete(self);if(index>=0)forget(&gongs[index]);return result;
}
/* Preserve all four public story flags and the stock singleton around only the
 * extra Medli's create/destructor, including asynchronous creation phases. */
typedef struct {u32 instance;u8 flags[4];} medli_story;
static medli_story save_story(void) {
    medli_story s;
    s.instance=WWHD_GAME_DATA(WWHD_ADDR_Medli_instance_pointer,u32);
    s.flags[0]=WWHD_GAME_DATA(WWHD_ADDR_Medli_flying,u8);
    s.flags[1]=WWHD_GAME_DATA(WWHD_ADDR_Medli_mirror,u8);
    s.flags[2]=WWHD_GAME_DATA(WWHD_ADDR_Medli_sea_talk,u8);
    s.flags[3]=WWHD_GAME_DATA(WWHD_ADDR_Medli_player_room,u8);return s;
}
static void restore_story(medli_story s) {
    WWHD_GAME_DATA(WWHD_ADDR_Medli_instance_pointer,u32)=s.instance;
    WWHD_GAME_DATA(WWHD_ADDR_Medli_flying,u8)=s.flags[0];
    WWHD_GAME_DATA(WWHD_ADDR_Medli_mirror,u8)=s.flags[1];
    WWHD_GAME_DATA(WWHD_ADDR_Medli_sea_talk,u8)=s.flags[2];
    WWHD_GAME_DATA(WWHD_ADDR_Medli_player_room,u8)=s.flags[3];
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daNpc_Md_c__create,s32,original_medli_create,(void* self));
WWHD_REPLACE(WWHD_ADDR_daNpc_Md_c__create,s32,dragon_medli_create,(void* self)) {
    if(!tagged_medli(self))return original_medli_create(self);
    u8* stage=wwhd_play_get()+WWHD_PLAY_START_STAGE_NAME_OFFSET;u8 previous[8];
    memcpy(previous,stage,8);memcpy(stage,"DQuest\0\0",8);
    medli_story story=save_story();int old_creating=creating_medli;creating_medli=1;
    s32 result=original_medli_create(self);
    creating_medli=old_creating;memcpy(stage,previous,8);restore_story(story);
    if(result==4){medli.actor=dragon_actor(self);*dragon_word(self,WWHD_OFFSET_actor_attention_flags)=0;}
    else if(result==5)forget(&medli);
    return result;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daNpc_Md_c__execute,s32,original_medli_execute,(void* self));
WWHD_REPLACE(WWHD_ADDR_daNpc_Md_c__execute,s32,dragon_medli_execute,(void* self)) {
    if(!tagged_medli(self))return original_medli_execute(self);
    daNpc_Md_c* actor=self;
    if(actor->mpMorf)wwhd_ext2_play_025E65FC((void*)actor->mpMorf,&dragon_actor(self)->eyePos,0,0);
    wwhd_daNpc_Md_c__setBaseMtx_02285A6C(self);return 1;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_daNpc_Md_dt,void,original_medli_delete,(void* self,s32 flags));
WWHD_REPLACE(WWHD_ADDR_daNpc_Md_dt,void,dragon_medli_delete,(void* self,s32 flags)) {
    if(!tagged_medli(self)){original_medli_delete(self,flags);return;}
    medli_story story=save_story();original_medli_delete(self,flags);restore_story(story);forget(&medli);
}
