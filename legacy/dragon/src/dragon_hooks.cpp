// MOD_DRAGON: runtime forwarding hooks for the addresses in hooks_dragon.txt.
// Every wrapper calls the original game function unchanged while WWHD_MOD_DRAGON is off.
// Link's execute (0240CDD0) is already hooked by the port's climb mod (climb.cpp); the
// integration patch chains dragon::execute into that hook instead of defining it here.
#include "dragon.h"
#include "dragon_quest.h"
extern "C" {
void f_025D63E8_orig(Cpu*); void hook_025D63E8(Cpu* c) { dragon::solid_heap(c,f_025D63E8_orig); }
void f_0212A728_orig(Cpu*); void hook_0212A728(Cpu* c) { dragon::actor_heap(c,f_0212A728_orig); }
void f_02129D28_orig(Cpu*); void hook_02129D28(Cpu* c) { dragon::actor_draw(c,f_02129D28_orig); }
void f_025E1F34_orig(Cpu*); void hook_025E1F34(Cpu* c) { dragon::song_judge(c,f_025E1F34_orig); }
void f_023E92A0_orig(Cpu*); void hook_023E92A0(Cpu* c) { dragon::tact(c,f_023E92A0_orig); }
void f_0212A830_orig(Cpu*); void hook_0212A830(Cpu* c) { dragon::actor_create(c,f_0212A830_orig); }
void f_0212A00C_orig(Cpu*); void hook_0212A00C(Cpu* c) { dragon::actor_execute(c,f_0212A00C_orig); }
void f_0212A6E4_orig(Cpu*); void hook_0212A6E4(Cpu* c) { dragon::actor_delete(c,f_0212A6E4_orig); }
void f_023FDA70_orig(Cpu*); void hook_023FDA70(Cpu* c) { dragon::position(c,f_023FDA70_orig); }
void f_02520460_orig(Cpu*); void hook_02520460(Cpu* c) { dragon::resource_phase(c,f_02520460_orig); }
void f_025204C8_orig(Cpu*); void hook_025204C8(Cpu* c) { dragon::resource_phase(c,f_025204C8_orig); }
void f_026066C4_orig(Cpu*); void hook_026066C4(Cpu* c) { dragon::resource_get(c,f_026066C4_orig); }
}

extern "C" {
void f_025BA7B0_orig(Cpu*); void hook_025BA7B0(Cpu* c) { dragon::quest::card_load(c,f_025BA7B0_orig); }
void f_025BAC50_orig(Cpu*); void hook_025BAC50(Cpu* c) { dragon::quest::card_new(c,f_025BAC50_orig); }
void f_0234C0B0_orig(Cpu*); void hook_0234C0B0(Cpu* c) { dragon::quest::gong_create(c,f_0234C0B0_orig); }
void f_0234C23C_orig(Cpu*); void hook_0234C23C(Cpu* c) { dragon::quest::gong_execute(c,f_0234C23C_orig); }
void f_0234C1C4_orig(Cpu*); void hook_0234C1C4(Cpu* c) { dragon::quest::gong_delete(c,f_0234C1C4_orig); }
void f_02286084_orig(Cpu*); void hook_02286084(Cpu* c) { dragon::quest::medli_create(c,f_02286084_orig); }
void f_02288B08_orig(Cpu*); void hook_02288B08(Cpu* c) { dragon::quest::medli_execute(c,f_02288B08_orig); }
void f_0228A254_orig(Cpu*); void hook_0228A254(Cpu* c) { dragon::quest::medli_delete(c,f_0228A254_orig); }
void f_025B7A2C_orig(Cpu*); void hook_025B7A2C(Cpu* c) { dragon::quest::collect(c,f_025B7A2C_orig); }
}
