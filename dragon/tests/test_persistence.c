#include <assert.h>
#include <string.h>
#include "wwhd_guest.h"
#undef WWHD_GAME_ORIGINAL
#undef WWHD_REPLACE
#define WWHD_GAME_ORIGINAL(addr,ret,name,params) ret name params
#define WWHD_REPLACE(addr,ret,name,params) static ret name params
#include "../src/persistence_impl.h"
static s32 stock_result;
static char saved[3][128];
static unsigned sizes[3],reads,writes;
s32 original_card_load(u32 self,u32 card,s32 slot) { (void)self;(void)card;(void)slot;return stock_result; }
s32 original_card_new(u32 self,u32 card,s32 slot) { (void)self;(void)card;(void)slot;return stock_result; }
void wwhd_log(const char* message) { (void)message; }
s32 wwhd_file_read(const char* name,void* output,u32 size) {
    unsigned slot=(unsigned)(name[17]-'0'); assert(slot<3); ++reads;
    unsigned count=sizes[slot]<size?sizes[slot]:size;
    memcpy(output,saved[slot],count); return (s32)count;
}
s32 wwhd_file_write(const char* name,const void* input,u32 size) {
    unsigned slot=(unsigned)(name[17]-'0'); assert(slot<3 && size<128);++writes;
    memcpy(saved[slot],input,size);sizes[slot]=size;return (s32)size;
}
int main(void) {
    dragon_quest_load(&dragon_quest,1,(dragon_progress){4,7});
    dragon_quest_persist(); assert(writes==1 && sizes[1]==24);
    stock_result=1;
    assert(dragon_card_load(0,0,2)==1 && reads==0 && dragon_quest.slot==1);
    stock_result=0;
    dragon_card_new(0,0,2);
    assert(dragon_quest.slot==1 && dragon_quest.progress.phase==4 && sizes[2]==24);
    dragon_card_load(0,0,2); assert(dragon_quest.slot==2 && dragon_quest.progress.phase==0);
    dragon_card_load(0,0,1); assert(dragon_quest.slot==1 && dragon_quest.progress.phase==4);
    dragon_quest_state state=dragon_quest;
    dragon_quest.progress=(dragon_progress){2,1};dragon_quest_persist();
    dragon_quest=state; /* Equivalent to restoring the mod region: no file callback. */
    assert(dragon_quest.progress.phase==4 && reads==2);
    dragon_card_load(0,0,-1);dragon_card_new(0,0,3);
    assert(dragon_quest.slot==1 && reads==2 && writes==3);
    strcpy(saved[0],"WWHD_DRAGON_QUEST 1\n4 6\n");sizes[0]=24;
    dragon_card_load(0,0,0);assert(dragon_quest.progress.phase==0);
    return 0;
}
