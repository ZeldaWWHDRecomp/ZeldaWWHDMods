/* SPDX-License-Identifier: MPL-2.0 */
#include "wwhd_guest.h"
#include "wwhd/functions.h"
#include "wwhd/link.h"
#include "wwhd/data.h"
#include "core.h"
#include "editor.h"

/* Only verified public game calls. Data bases use SDK relocations for USA/EU. */
WWHD_GAME_FUNC(0x023E6380, s32, spin_attack, (void*, s32));
WWHD_GAME_FUNC(0x023D794C, s32, equipping, (void*));
WWHD_GAME_FUNC(0x02443728, s32, guarding, (void*));
#define PLAY (wwhd_play_get())
#define U16(p,o) (*(volatile u16*)((u8*)(p)+(o)))
#define U32(p,o) (*(volatile u32*)((u8*)(p)+(o)))

static EcState state;
static int registered, ready, mask, claims_valid;
static EcRect rects[6], claimed[6];
static EcEditor editor;
static EcLayoutFile layout_file[2]; /* host services require mod-owned buffers */
static const char* const layout_names[]={"macro-layout-a.bin","macro-layout-b.bin"};
static int editor_visible=1, touch_enabled, release_input;
static void load_layout(void) {
    editor.saved=ee_defaults(); editor.sequence=0; editor.available=15; ee_close(&editor);
    for(int i=0;i<2;++i) {
        EcLayoutFile* f=&layout_file[i];
        if(ee_decode(f,wwhd_file_read(layout_names[i],f,sizeof(*f))) &&
           (!editor.sequence || (s32)(f->sequence-editor.sequence)>0)) {
            editor.saved=f->layout; editor.sequence=f->sequence;
        }
    }
}
static int save_layout(void) {
    unsigned sequence=editor.sequence+1; if(!sequence) sequence=1;
    EcLayoutFile* f=&layout_file[sequence&1];
    *f=(EcLayoutFile){0x45434C59u,2,sequence,0,editor.draft}; f->checksum=ee_hash(f);
    if(wwhd_file_write(layout_names[sequence&1],f,sizeof(*f))!=(s32)sizeof(*f)) {
        editor.error=1; return 0;
    }
    editor.saved=editor.draft; editor.sequence=sequence; return 1;
}
static unsigned claimed_mask;
static unsigned long long last_step, hud_epoch;
static u32 player_id;
static wwhd_input_state sampled; /* service buffers must belong to the mod */

static int object(u32 p, u32 bytes) { return !(p&3) && p >= 0x10000000 && p <= 0x50000000-bytes; }
static int is_menu(void) {
    if (WWHD_GAME_DATA(0x101EA069,u8) || WWHD_GAME_DATA(0x101F8348,u32)==2) return 1;
    u32 ui=WWHD_GAME_DATA(0x101F8344,u32);
    if (!object(ui,0x24c)) return 0;
    u32 current=U32(ui,0x18);
    u32 desc=object(current,12) ? U32(current,8) : U32(ui,0x14);
    u32 bag=U32(ui,0x17c);
    if (object(bag,0x58) && U32(bag,0x54)<3) return 1;
    /* Exact relocated scene descriptors from upstream gfx/auto_pip.h. */
#define MENU(a) if (desc==(u32)&WWHD_GAME_DATA(a,u8)) return 1
    MENU(0x1049EC84); MENU(0x1049ECB0); MENU(0x1049ECDC); MENU(0x1049ED08);
    MENU(0x1049ED34); MENU(0x1049ED60); MENU(0x1049ED8C); MENU(0x1049EDB8);
    MENU(0x1049EDE4); MENU(0x1049EE10); MENU(0x1049EE3C); MENU(0x1049EEEC);
    MENU(0x1049EF18); MENU(0x1049EF44); MENU(0x1049EF70); MENU(0x1049EF9C);
    MENU(0x1049EFC8); MENU(0x1049EFF4); MENU(0x1049F020); MENU(0x1049F04C);
    MENU(0x1049F078); MENU(0x1049F0A4); MENU(0x1049F0D0); MENU(0x1049F0FC);
    MENU(0x1049F1D8); MENU(0x1049F204); MENU(0x1049F288); MENU(0x1049F2B4);
    MENU(0x1049F2E0); MENU(0x1049F30C); MENU(0x1049F338);
    MENU(0x1049F498); MENU(0x1049F4C4); MENU(0x1049F4F0);
#undef MENU
    return 0;
}
static int playable(void) {
    u8* p=PLAY;
    if (!p) return 0;
    u32 link=U32(p,WWHD_PLAY_PLAYER_OFFSET);
    if (!object(link,0x8284) || !U32(link,0) || !U32(link,4) || U32(link,4)==0xffffffff) return 0;
    if (!p[WWHD_PLAY_START_STAGE_NAME_OFFSET] || (p[WWHD_PLAY_START_STAGE_NAME_OFFSET]=='s' && p[WWHD_PLAY_START_STAGE_NAME_OFFSET+1]=='e' && p[WWHD_PLAY_START_STAGE_NAME_OFFSET+2]=='a' && p[WWHD_PLAY_START_STAGE_NAME_OFFSET+3]=='_' && p[WWHD_PLAY_START_STAGE_NAME_OFFSET+4]=='T') ||
        (p[WWHD_PLAY_START_STAGE_NAME_OFFSET]=='N' && p[WWHD_PLAY_START_STAGE_NAME_OFFSET+1]=='a' && p[WWHD_PLAY_START_STAGE_NAME_OFFSET+2]=='m' && p[WWHD_PLAY_START_STAGE_NAME_OFFSET+3]=='e')) return 0;
    if (p[WWHD_PLAY_ENABLE_NEXT_STAGE_OFFSET] || p[WWHD_PLAY_EVENT_RUNNING_OFFSET] || p[0x5BB2] || p[0x5BB3] || WWHD_GAME_DATA(0x101F36CC,u32) || is_menu()) return 0;
    u32 save=WWHD_GAME_DATA(WWHD_ADDR_dComIfG_save_info_pointer,u32);
    if (!object(save,0x24) || !U16(save,0x22)) return 0;
    /* Stop during first-person aiming, climbing, swimming, sailing and conducting. */
    const u32 special0=0x100|0x1000|0x2000|0x4000|0x10000|0x20000|0x80000|0x100000|0x200000|0x400000|0x800000|0x2000000|0x4000000|0x8000000;
    if (U32(p,0x5CD8)&special0 || U32(p,0x5CDC)&(1|8|0x20|0x400)) return 0;
    if (((daPy_lk_c*)link)->mModeFlg&(0x20|0x200|0x800|0x2000|0x10000|0x40000|0x100000|0x200000|0x400000|0x800000|0x1000000|0x20000000)) return 0;
    return 1;
}
static void clear_claims(void) {
    if (!claims_valid) return;
    for (u32 i=0;i<6;++i) wwhd_touch_claim(i,0,0,0,0);
    claims_valid=0; claimed_mask=0;
}
static void update_claims(void) {
    unsigned wanted=ready ? (editor.open?32u:((unsigned)mask|(editor_visible?16u:0u))) : 0;
    if (!wanted) { clear_claims(); return; }
    int changed=!claims_valid || wanted!=claimed_mask;
    for (int i=0;i<6;++i) {
        EcRect a=rects[i], b=claimed[i];
        if ((wanted&(1u<<i)) && (a.x!=b.x || a.y!=b.y || a.w!=b.w || a.h!=b.h)) changed=1;
    }
    if (!changed) return; /* Do not steal overlapping claims back every frame. */
    ec_cancel(&state);
    for (u32 i=0;i<6;++i) {
        EcRect r=rects[i]; claimed[i]=r;
        if (wanted&(1u<<i)) wwhd_touch_claim(i,r.x-r.w/2,r.y-r.h/2,r.x+r.w/2,r.y+r.h/2);
        else wwhd_touch_claim(i,0,0,0,0);
    }
    claims_valid=1; claimed_mask=wanted;
}
static float number(const char* id,float fallback,float lo,float hi) {
    return ec_clamp((float)wwhd_config_float(id,fallback),lo,hi);
}
static void refresh(void) {
    unsigned long long step=wwhd_logic_step(), epoch=wwhd_hud_epoch();
    if (step!=last_step || epoch!=hud_epoch) {
        if (epoch!=hud_epoch || step<last_step || step-last_step>1) {
            ec_cancel(&state); clear_claims();
            if(epoch!=hud_epoch) load_layout();
            else ee_close(&editor);
        } else ec_tick(&state,wwhd_logic_dt());
        last_step=step; hud_epoch=epoch;
        touch_enabled=wwhd_config_bool("touch-enabled",0);
        if (!touch_enabled) release_input=0;
        editor_visible=wwhd_config_bool("editor-visible",1);
        if(!editor_visible && editor.open) ee_close(&editor);
        rects[4]=(EcRect){number("editor-x",0,-570,570),number("editor-y",260,-320,320),96,48};
        rects[5]=(EcRect){0,0,1280,720};
    }
    int next=touch_enabled && playable();
    u32 id=next ? U32(U32(PLAY,WWHD_PLAY_PLAYER_OFFSET),4) : 0;
    if (!next || id!=player_id) { ec_cancel(&state); ee_close(&editor); }
    editor.available=15; /* four local macro buttons only */
    mask=0;
    for(int i=0;i<4;++i) {
        EcPlacement p=(editor.open?editor.draft:editor.saved).button[i];
        rects[i]=ee_rect(p); if(p.visible) mask|=1<<i;
    }
    player_id=id; ready=next;
    update_claims();
}
static void touch(const wwhd_touch_event* event) {
    refresh(); if(!ready) return;
    float x=event->x, y=event->y;
    if(editor.open) {
        int action=ee_touch(&editor,event->kind,x,y);
        if(action==EE_CANCEL) ee_close(&editor);
        else if(action==EE_SAVE) { if(save_layout()) ee_close(&editor); }
        else if(action>=0) ee_apply(&editor,action);
        ec_cancel(&state); refresh(); return;
    }
    if(event->kind==WWHD_TOUCH_DOWN && editor_visible && ec_inside(rects[4],x,y)) {
        ee_begin(&editor); release_input=1; ec_cancel(&state); refresh(); return;
    }
    switch(event->kind) {
        case WWHD_TOUCH_DOWN: {
            int hit=EC_NONE;
            for(int i=0;i<4;++i) if((mask&(1<<i)) && ec_inside(rects[i],x,y)) hit=i;
            if(hit>=0 && hit<4) {
                wwhd_input_read(&sampled);
                /* Attack macros need a drawn sword; dodge works without it. */
                if(hit==EC_DODGE || ((daPy_lk_c*)U32(PLAY,WWHD_PLAY_PLAYER_OFFSET))->mEquipItem==0x103) {
                    if(hit!=EC_SPIN || state.macro!=EC_SPIN || !ec_active(&state))
                        ec_start(&state,hit,sampled.lx);
                }
            }
            break;
        }
        case WWHD_TOUCH_CANCEL: ec_cancel(&state); break;
    }
}
static void filter(wwhd_input_state* pad) {
    refresh(); if(!ready) return;
    if(editor.open || release_input) {
        int neutral=!pad->buttons && pad->lx>=-.15f && pad->lx<=.15f && pad->ly>=-.15f && pad->ly<=.15f &&
            pad->rx>=-.15f && pad->rx<=.15f && pad->ry>=-.15f && pad->ry<=.15f;
        if(!editor.open && neutral) release_input=0;
        pad->buttons=0; pad->lx=pad->ly=pad->rx=pad->ry=0; return;
    }
    /* Always preserve real buttons. The macro only supplies additional holds. */
    pad->buttons |= ec_buttons(&state);
    ec_stick(&state,&pad->lx,&pad->ly);
}
/* Four 256x256 RGBA medallions use 1 MiB of decoded texture memory. */
static const char* const icon_paths[] = {
    "assets/combat_jump_attack.png", "assets/combat_vertical_slash.png",
    "assets/combat_spin_attack.png", "assets/combat_dodge.png"
};
static u32 icons[4];
static unsigned icon_attempts;
static unsigned long long icon_epoch=~0ull;
static void sync_icons(void) {
    unsigned long long epoch=wwhd_hud_epoch();
    if(epoch!=icon_epoch) {
        for(int i=0;i<4;++i) icons[i]=0; /* old handles are already invalid */
        icon_attempts=0; icon_epoch=epoch;
    }
    for(int i=0;i<4;++i) {
        EcPlacement p=(editor.open?editor.draft:editor.saved).button[i];
        int needed=touch_enabled && p.icons && (p.visible || editor.open);
        if(!needed) { if(icons[i]) wwhd_hud_release(icons[i]); icons[i]=0; icon_attempts&=~(1u<<i); }
        else if(!(icon_attempts&(1u<<i))) {
            icons[i]=wwhd_hud_texture(WWHD_HUD_PACKAGE,icon_paths[i]); icon_attempts|=1u<<i;
        }
    }
}
#include "editor_view.h"
static void editor_button(u32 list,int i) {
    if(i>=4) return; /* rendered by its owning mod */
    EcPlacement p=(editor.open?editor.draft:editor.saved).button[i];
    ee_draw_button(list,p,names[i],icons[i],
        editor.open,editor.selected==i,state.macro==i);
}
static void hud(u32 list) {
    refresh(); if(!ready) return;
    sync_icons();
    if(editor.open) { editor_hud(list); return; }
    for(int i=0;i<4;++i) if(mask&(1<<i)) editor_button(list,i);
    if(editor_visible) box(list,rects[4],"Edit",0x23445CD0);
}
/* Public HD proc IDs: WAIT, MOVE, ATN_MOVE, ATN_ACTOR_WAIT/MOVE,
 * WAIT_TURN and MOVE_TURN. Never interrupt damage, jumps, rolls, another attack,
 * item use or scripted actions. Recheck at consumption, not in the pad callback.
 * procCutTurn_init(link, 1) is the native quick-spin path in checkNextAction. */
static int can_spin(daPy_lk_c* link) {
    u32 proc=link->mCurProc;
    if(proc!=4 && proc!=6 && proc!=7 && proc!=8 && proc!=9 && proc!=0x17 && proc!=0x18) return 0;
    if(link->mEquipItem!=0x103 || U32(link->m8260,0) ||
       U32(link,0x65A4) || U32(link,0x6594) || (U32(link,0x3B8)&0x20) ||
       WWHD_GAME_DATA(0x101CEF16,u16)==1) return 0;
    return !equipping(link) && !guarding(link);
}
WWHD_HOOK(WWHD_ADDR_daPy_Execute,enhanced_register,(void* link)) {
    if(!registered) {
        ec_init(&state); load_layout(); last_step=~0ull;
        wwhd_touch_register(touch); wwhd_input_filter_register(filter);
        wwhd_hud_register(hud,WWHD_HUD_TV); registered=1;
        wwhd_log("Enhanced Controls: experimental guest mod ready");
    }
    refresh();
    /* Execute once on Link's game update, never from a touch/HUD/input callback.
     * A refused request is discarded instead of firing later after recovery. */
    if(ec_take_spin(&state)) {
        if(editor.open || !ready || (u32)link!=U32(PLAY,WWHD_PLAY_PLAYER_OFFSET) || !can_spin(link) || !spin_attack(link,1))
            state.macro=EC_NONE;
    }
}
