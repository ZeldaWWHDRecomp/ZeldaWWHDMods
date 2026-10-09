#include <assert.h>
#include <string.h>
#include "../src/hud_impl.h"
dragon_quest_state dragon_quest;
static wwhd_hud_element commands[12];
static unsigned count;
u32 wwhd_hud_register(void (*callback)(u32),u32 screen) {(void)callback;assert(screen==WWHD_HUD_TV);return 1;}
u32 wwhd_hud_emit(u32 list,const wwhd_hud_element* command) {assert(list==1 && count<12);commands[count++]=*command;return 1;}
static void check(void) {
    count=0;draw(1);assert(count>=4);
    wwhd_hud_element panel=commands[0];
    assert(panel.kind==WWHD_HUD_RECT && panel.w>0 && panel.h>0);
    assert(panel.x>=0 && panel.y>=0 && panel.x+panel.w<=1280 && panel.y+panel.h<=720);
    /* Actual default gc-minimap occupied box: x=6, bottom=6, side=225.
       Quest, song and flight panels must coexist, not merely their titles. */
    assert(panel.x>=231 || panel.x+panel.w<=6 || panel.y>=714 || panel.y+panel.h<=489);
    for(unsigned i=1;i<count;++i) {
        wwhd_hud_element e=commands[i];
        assert(e.kind==WWHD_HUD_TEXT && e.text && e.text_bytes==strlen(e.text));
        assert(e.x>panel.x && e.x<panel.x+panel.w);
        assert(e.y>=panel.y && e.y+e.size<=panel.y+panel.h);
        assert(e.thickness>0 && e.u1==1 && e.v1==1);
    }
}
int main(void) {
    dragon_hud.visible=1;
    for(int phase=DRAGON_IDLE;phase<=DRAGON_SONG;++phase) {
        dragon_hud.phase=phase;
        for(int quest=0;quest<=1;++quest) {
            dragon_hud.quest_visible=quest;
            for(int dialog=0;dialog<=5;++dialog) {
                dragon_quest.dialog=dialog;
                for(unsigned progress=0;progress<=4;++progress) {
                    dragon_quest.progress.phase=progress;dragon_quest.progress.mask=7;check();
                }
            }
        }
    }
}
