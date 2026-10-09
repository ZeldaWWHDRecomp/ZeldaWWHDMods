/* Per-mod files only. Guest state is restored with full states; file reads happen
 * solely after a successful stock card load, never after a HUD epoch change. */
#include "quest.h"
#include "wwhd/functions.h"

dragon_quest_state dragon_quest={.slot=-1};
static char filename[]="dragon-quest-slot0.txt";
static char progress_bytes[128];
static void slot_path(int slot) { filename[17]=(char)('0'+slot); }
void dragon_quest_persist(void) {
    if (dragon_quest.slot<0 || dragon_quest.slot>2) return;
    slot_path(dragon_quest.slot);
    unsigned length=dragon_progress_encode(dragon_quest.progress,progress_bytes);
    if (wwhd_file_write(filename,progress_bytes,length)!=(s32)length)
        wwhd_log("[dragon] quest progress write failed");
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_dSv_info_c_card_to_memory, s32, original_card_load,
                   (u32 self,u32 card,s32 slot));
WWHD_REPLACE(WWHD_ADDR_dSv_info_c_card_to_memory,s32,dragon_card_load,
             (u32 self,u32 card,s32 slot)) {
    s32 result=original_card_load(self,card,slot);
    if (!result && slot>=0 && slot<3) {
        dragon_progress p={0,0};
        slot_path(slot);
        s32 length=wwhd_file_read(filename,progress_bytes,sizeof progress_bytes);
        if (length>0 && length<(s32)sizeof progress_bytes)
            dragon_progress_decode(progress_bytes,(unsigned)length,&p);
        dragon_quest_load(&dragon_quest,slot,p);
    }
    return result;
}
WWHD_GAME_ORIGINAL(WWHD_ADDR_dSv_info_c_initdata_to_card,s32,original_card_new,
                   (u32 self,u32 card,s32 slot));
WWHD_REPLACE(WWHD_ADDR_dSv_info_c_initdata_to_card,s32,dragon_card_new,
             (u32 self,u32 card,s32 slot)) {
    s32 result=original_card_new(self,card,slot);
    if (!result && slot>=0 && slot<3) {
        /* Initializing another card slot must not switch the active quest. */
        slot_path(slot);
        unsigned length=dragon_progress_encode((dragon_progress){0,0},progress_bytes);
        if (wwhd_file_write(filename,progress_bytes,length)!=(s32)length)
            wwhd_log("[dragon] new-slot quest reset failed");
    }
    return result;
}
