#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int16_t s16; typedef float f32;
#define LINK 0x20000000u
#define BOX 0x21000000u
#define OTHER 0x22000000u
static unsigned char link_memory[0x7000], box_memory[0x700], other_memory[0x100];
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
            BP_AT(s16, BOX + 0x644) = (s16)n;
            int ticks = 0;
            while (BP_AT(s16, BOX + 0x644) > 0) {
                qol_block_push_tick((void*)(uintptr_t)BOX);
                --BP_AT(s16, BOX + 0x644); /* The unchanged native decrement. */
                assert(++ticks <= n);
            }
            assert(BP_AT(s16, BOX + 0x644) == 0);
            int expected = speed == 0 ? n : speed == 1 ? (2 * n + 2) / 3 : (n + 1) / 2;
            assert(ticks == expected);
        }
    }
    for (int invalid = -32768; invalid <= 0; ++invalid) {
        BP_AT(s16, BOX + 0x644) = (s16)invalid;
        qol_block_push_tick((void*)(uintptr_t)BOX);
        assert(BP_AT(s16, BOX + 0x644) == invalid);
    }
}
static void synchronization(void) {
    const int durations[] = {12, 15, 18, 20, 24, 25, 30, 36, 40, 48, 60, 72, 90};
    for (unsigned row = 0; row < 13; ++row) for (int pull = 0; pull < 2; ++pull)
    for (int speed = 1; speed <= 2; ++speed) {
        choose(speed == 1 ? "1.5x" : "2x");
        const int n = durations[row]; float box = 0, player = pull ? -70.f : 70.f;
        const float gap = player;
        BP_AT(s16, BOX + 0x644) = (s16)n;
        while (BP_AT(s16, BOX + 0x644) > 0) {
            float previous = box;
            qol_block_push_tick((void*)(uintptr_t)BOX);
            int left = --BP_AT(s16, BOX + 0x644);
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
        BP_AT(s16,LINK+0x69AC)=(s16)proc;
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
    choose("2x");BP_AT(s16,LINK+0x69AC)=0x33;
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
        BP_AT(s16,LINK+0x69AC)=(s16)proc;BP_AT(f32,LINK+0x5898)=1.f;
        qol_block_push_animation_before((void*)(uintptr_t)LINK);
        qol_block_push_animation_after((void*)(uintptr_t)LINK);
        assert(BP_AT(f32,LINK+0x5898)==1.f);
    }
    choose("Off");accesses=0;
    qol_block_push_tick(0);qol_block_push_animation_before(0);qol_block_push_animation_after(0);
    assert(!accesses);
    choose("unknown");assert(!block_push_choice);
}
int main(void) {countdowns();synchronization();animation();puts("block push Link-only contracts passed");}
