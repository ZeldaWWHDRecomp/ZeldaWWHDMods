#include <assert.h>
#include <string.h>
#include "../src/quest_state.h"
int main(void) {
    dragon_quest_state q={.slot=-1};
    dragon_song_state song={0};
    dragon_progress p={0,0}, decoded={4,7};
    char text[25];
    assert(!dragon_quest_invite(&q,1,1));
    dragon_quest_load(&q,1,p);
    assert(!dragon_quest_invite(&q,0,1));
    assert(dragon_quest_invite(&q,1,1));
    assert(!dragon_quest_chime(&q,0));
    assert(!dragon_quest_learn(&q));
    assert(dragon_quest_accept(&q));
    assert(!dragon_quest_chime(&q,3));
    assert(dragon_quest_chime(&q,2));
    assert(!dragon_quest_chime(&q,2));
    assert(dragon_quest_chime(&q,0));
    assert(dragon_quest_chime(&q,1));
    assert(q.progress.phase==3 && q.progress.mask==7);
    assert(dragon_quest_learn(&q));
    assert(!dragon_quest_learn(&q));
    for (unsigned phase=0;phase<6;++phase) for(unsigned mask=0;mask<9;++mask) {
        p=(dragon_progress){phase,mask};
        unsigned length=dragon_progress_encode(p,text);
        assert(length==24 && dragon_progress_decode(text,length,&decoded));
        dragon_progress expected=dragon_progress_normalize(p);
        assert(decoded.phase==expected.phase && decoded.mask==expected.mask);
    }
    const char* malformed[]={"", "WWHD_DRAGON_QUEST 2\n4 7\n", "WWHD_DRAGON_QUEST 1\n4 6\n", "WWHD_DRAGON_QUEST 1\n1 1\n", "WWHD_DRAGON_QUEST 1\n2 8\n", "WWHD_DRAGON_QUEST 1\n2 1junk", "WWHD_DRAGON_QUEST 1\n42949672960 0", "WWHD_DRAGON_QUEST1 2 0"};
    for(unsigned i=0;i<sizeof(malformed)/sizeof(*malformed);++i)
        assert(!dragon_progress_decode(malformed[i],(unsigned)strlen(malformed[i]),&decoded));
    assert(dragon_progress_decode("WWHD_DRAGON_QUEST 1\n2 7\n",24,&decoded));
    assert(decoded.phase==3);
    unsigned notes[]={1,2,1,4,3,2};
    for(unsigned i=0;i<6;++i) assert(dragon_song_beat(&song,i,notes[i],6,1)==(i==5));
    assert(!dragon_song_beat(&song,6,2,6,1) && !song.matched);
    for(unsigned i=0;i<6;++i) { assert(!dragon_song_beat(&song,i,notes[i],3,1)); assert(!song.matched); }
    assert(!dragon_song_beat(&song,0,1,6,1));
    assert(!dragon_song_beat(&song,1,4,6,1) && !song.matched);
    assert(!dragon_song_beat(&song,0,1,6,0) && !song.matched);
    /* Full-state restoration is a copy, not a reread of newer on-disk progress. */
    dragon_quest_state snapshot=q;
    dragon_quest_load(&q,2,(dragon_progress){0,0});
    q=snapshot;
    assert(q.slot==1 && q.progress.phase==4);
    return 0;
}
