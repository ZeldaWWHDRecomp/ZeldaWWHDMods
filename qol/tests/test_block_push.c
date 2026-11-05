#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int16_t s16; typedef int32_t s32; typedef float f32;
/* Fixtures of the SDK views block_push.c names (wwhd/link.h, wwhd/animation.h), at the HD offsets the game's
 * code uses (the GameCube offsets that the decomp's field names still carry are wrong here). */
typedef union J3DFrameCtrl {
    u8 bytes[0x10];
    struct __attribute__((packed)) { f32 mRate; };
    struct __attribute__((packed)) { u8 _pad_mAttribute[0xE]; u8 mAttribute; };
} J3DFrameCtrl;
typedef union daPy_lk_c {
    u8 bytes[0x8284];
    struct __attribute__((packed)) { u8 _pad_mAnmRatioUnder[0x57F8]; u8 mAnmRatioUnder[0x20]; };
    struct __attribute__((packed)) { u8 _pad_mAnmRatioUpper[0x5818]; u8 mAnmRatioUpper[0x30]; };
    struct __attribute__((packed)) { u8 _pad_mFrameCtrlUnder[0x5898]; J3DFrameCtrl mFrameCtrlUnder[2]; };
    struct __attribute__((packed)) { u8 _pad_mFrameCtrlUpper[0x58B8]; J3DFrameCtrl mFrameCtrlUpper[3]; };
    struct __attribute__((packed)) { u8 _pad_mCurProc[0x65F0]; s32 mCurProc; };
} daPy_lk_c;
_Static_assert(__builtin_offsetof(daPy_lk_c, mCurProc) == 0x65F0, "HD daPy_lk_c.mCurProc");
_Static_assert(__builtin_offsetof(J3DFrameCtrl, mAttribute) == 0xE, "HD J3DFrameCtrl.mAttribute");
/* HD fields the hooks must use; GameCube-offset fields they must leave alone */
#define WALK 0x75C       /* daObjMovebox::Act_c walk countdown: mode_walk 023798C8 lha / 02379928 sth */
#define PROC 0x65F0      /* daPy_lk_c.mCurProc (s32) */
#define GC_WALK 0x644
#define GC_PROC 0x69AC
#define LINK 0x20000000u
#define BOX 0x21000000u
#define OTHER 0x22000000u
static unsigned char link_memory[0x7000], box_memory[0x800], other_memory[0x100];
static unsigned accesses;
static const char* option;
static void* memory(u32 at, unsigned size) {
    accesses++;
    if (at >= LINK && at + size <= LINK + sizeof link_memory) return link_memory + at - LINK;
    if (at >= BOX && at + size <= BOX + sizeof box_memory) return box_memory + at - BOX;
    if (at >= OTHER && at + size <= OTHER + sizeof other_memory) return other_memory + at - OTHER;
    assert(!"unexpected memory access"); return 0;
}
#define BP_AT(t, a) (*(t*)memory((a), sizeof(t)))
#define WWHD_HOOK(addr, name, args) static void name args
#define WWHD_HOOK_RETURN(addr, name, args) static void name args
static int qol_guest_ptr(u32 p) { return p >= 0x10000000u && p < 0x50000000u; }
static int wwhd_config_string(const char* key, char* dst, unsigned size) {
    assert(!strcmp(key, "block_push_speed")); snprintf(dst, size, "%s", option); return 1;
}
#include "../src/block_push.c"
static void choose(const char* value) { option = value; block_push_option_changed(); }
static void countdowns(void) {
    /* Check total native steps/endpoints independently of the acceleration rule. */
    for (int speed = 0; speed < 3; ++speed) {
        choose(speed == 0 ? "Off" : speed == 1 ? "1.5x" : "2x");
        for (int n = 1; n <= 4096; ++n) {
            BP_AT(s16, BOX + WALK) = (s16)n;
            int ticks = 0;
            while (BP_AT(s16, BOX + WALK) > 0) {
                qol_block_push_tick((void*)(uintptr_t)BOX);
                --BP_AT(s16, BOX + WALK); /* The unchanged native decrement. */
                assert(++ticks <= n);
            }
            assert(BP_AT(s16, BOX + WALK) == 0);
            int expected = speed == 0 ? n : speed == 1 ? (2 * n + 2) / 3 : (n + 1) / 2;
            assert(ticks == expected);
        }
    }
    for (int invalid = -32768; invalid <= 0; ++invalid) {
        BP_AT(s16, BOX + WALK) = (s16)invalid;
        qol_block_push_tick((void*)(uintptr_t)BOX);
        assert(BP_AT(s16, BOX + WALK) == invalid);
    }
}
static void synchronization(void) {
    const int durations[] = {12, 15, 18, 20, 24, 25, 30, 36, 40, 48, 60, 72, 90};
    for (unsigned row = 0; row < 13; ++row) for (int pull = 0; pull < 2; ++pull)
    for (int speed = 1; speed <= 2; ++speed) {
        choose(speed == 1 ? "1.5x" : "2x");
        const int n = durations[row]; float box = 0, player = pull ? -70.f : 70.f;
        const float gap = player;
        BP_AT(s16, BOX + WALK) = (s16)n;
        while (BP_AT(s16, BOX + WALK) > 0) {
            float previous = box;
            qol_block_push_tick((void*)(uintptr_t)BOX);
            int left = --BP_AT(s16, BOX + WALK);
            box = (1.f + cosf((float)left * 3.14159265358979323846f / n)) * 50.f;
            if (pull) box = -box;
            /* Native setPushPullKeepData follows the actual block delta. */
            player += box - previous;
            assert(fabsf((player - box) - gap) < .0001f);
        }
        assert(fabsf(box - (pull ? -100.f : 100.f)) < .0001f);
    }
}
static void animation(void) {
    memset(link_memory,0,sizeof link_memory);
    BP_AT(u16,LINK+0xE)=0xA8;
    BP_AT(u32,LINK+0x57FC)=0x1000;BP_AT(u32,LINK+0x581C)=0x2000;
    for(int proc=0x33;proc<=0x34;++proc)for(int speed=1;speed<=2;++speed) {
        choose(speed==1?"1.5x":"2x");
        BP_AT(s32,LINK+PROC)=proc;
        for(unsigned i=0;i<2;++i) {
            u32 at=LINK+0x5898+i*0x20;
            BP_AT(u8,at+0xE)=2;BP_AT(f32,at)=1.f+i;
        }
        qol_block_push_animation_before((void*)(uintptr_t)LINK);
        for(unsigned i=0;i<2;++i)assert(BP_AT(f32,LINK+0x5898+i*0x20)==(1.f+i)*(speed==1?1.5f:2.f));
        /* Both rates are present at the native Link-only update's consumption
         * points. Nested same-Link entry must not multiply them a second time. */
        qol_block_push_animation_before((void*)(uintptr_t)LINK);
        assert(block_push_rate.depth==2);
        qol_block_push_animation_before((void*)(uintptr_t)OTHER);
        qol_block_push_animation_after((void*)(uintptr_t)OTHER);
        assert(block_push_rate.depth==2);
        qol_block_push_animation_after((void*)(uintptr_t)LINK);
        assert(block_push_rate.depth==1);
        qol_block_push_animation_after((void*)(uintptr_t)LINK);
        assert(!block_push_rate.depth);
        assert(BP_AT(f32,LINK+0x5898)==1.f && BP_AT(f32,LINK+0x58B8)==2.f);
        qol_block_push_animation_before((void*)(uintptr_t)LINK);
        BP_AT(f32,LINK+0x5898)=0.f;BP_AT(f32,LINK+0x58B8)=.75f;
        qol_block_push_animation_after((void*)(uintptr_t)LINK);
        assert(BP_AT(f32,LINK+0x5898)==0.f && BP_AT(f32,LINK+0x58B8)==.75f);
    }
    /* An absent or shared upper animation pack is not advanced by animeUpdate;
     * its unused controller must not even have its rate read or changed. */
    choose("2x");BP_AT(s32,LINK+PROC)=0x33;
    for(unsigned pack=0;pack<=0x1000;pack+=0x1000) {
        BP_AT(u32,LINK+0x581C)=pack;
        BP_AT(f32,LINK+0x5898)=1.f;BP_AT(f32,LINK+0x58B8)=3.f;
        qol_block_push_animation_before((void*)(uintptr_t)LINK);
        assert(BP_AT(f32,LINK+0x5898)==2.f && BP_AT(f32,LINK+0x58B8)==3.f);
        qol_block_push_animation_after((void*)(uintptr_t)LINK);
        assert(BP_AT(f32,LINK+0x5898)==1.f && BP_AT(f32,LINK+0x58B8)==3.f);
    }
    BP_AT(u32,LINK+0x581C)=0x2000;
    BP_AT(f32,LINK+0x5898)=1.f;BP_AT(f32,LINK+0x58B8)=2.f;
    qol_block_push_animation_before((void*)(uintptr_t)LINK);
    choose("Off"); /* Live option change must not strand an injected rate. */
    qol_block_push_animation_after((void*)(uintptr_t)LINK);
    assert(BP_AT(f32,LINK+0x5898)==1.f && BP_AT(f32,LINK+0x58B8)==2.f);
    choose("2x");
    for(int proc=0;proc<0x100;++proc)if(proc!=0x33&&proc!=0x34) {
        BP_AT(s32,LINK+PROC)=proc;BP_AT(f32,LINK+0x5898)=1.f;
        qol_block_push_animation_before((void*)(uintptr_t)LINK);
        qol_block_push_animation_after((void*)(uintptr_t)LINK);
        assert(BP_AT(f32,LINK+0x5898)==1.f);
    }
    choose("Off");accesses=0;
    qol_block_push_tick(0);qol_block_push_animation_before(0);qol_block_push_animation_after(0);
    assert(!accesses);
    choose("unknown");assert(!block_push_choice);
}
/* The HD box field counts down; whatever HD keeps at the GameCube offset +0x644 is never touched. Link's
 * procedure is read from HD mCurProc only: a push/pull value at the GameCube offset changes nothing. */
static void hd_offsets(void) {
    choose("2x");
    memset(box_memory,0,sizeof box_memory);
    BP_AT(s16,BOX+GC_WALK)=0x1234; BP_AT(s16,BOX+WALK)=20;
    qol_block_push_tick((void*)(uintptr_t)BOX);
    assert(BP_AT(s16,BOX+WALK)==19 && BP_AT(s16,BOX+GC_WALK)==0x1234);
    BP_AT(s16,BOX+GC_WALK)=20; BP_AT(s16,BOX+WALK)=0;
    qol_block_push_tick((void*)(uintptr_t)BOX);
    assert(BP_AT(s16,BOX+GC_WALK)==20 && BP_AT(s16,BOX+WALK)==0);
    memset(link_memory,0,sizeof link_memory);
    BP_AT(u16,LINK+0xE)=0xA8; BP_AT(u32,LINK+0x57FC)=0x1000; BP_AT(u32,LINK+0x581C)=0x2000;
    BP_AT(u8,LINK+0x5898+0xE)=2; BP_AT(f32,LINK+0x5898)=1.f;
    BP_AT(s16,LINK+GC_PROC)=0x33; BP_AT(s32,LINK+PROC)=0x0;
    qol_block_push_animation_before((void*)(uintptr_t)LINK);
    assert(BP_AT(f32,LINK+0x5898)==1.f);
    qol_block_push_animation_after((void*)(uintptr_t)LINK);
    for (int proc=0x33;proc<=0x34;++proc) {
        BP_AT(s16,LINK+GC_PROC)=0; BP_AT(s32,LINK+PROC)=proc;
        qol_block_push_animation_before((void*)(uintptr_t)LINK);
        assert(BP_AT(f32,LINK+0x5898)==2.f);
        qol_block_push_animation_after((void*)(uintptr_t)LINK);
        assert(BP_AT(f32,LINK+0x5898)==1.f);
    }
    /* mCurProc is 32 bits: a push value in its high half is another procedure */
    BP_AT(s32,LINK+PROC)=0x330000;
    qol_block_push_animation_before((void*)(uintptr_t)LINK);
    assert(BP_AT(f32,LINK+0x5898)==1.f);
    qol_block_push_animation_after((void*)(uintptr_t)LINK);
}
int main(void) {hd_offsets();countdowns();synchronization();animation();puts("block push Link-only contracts passed");}
