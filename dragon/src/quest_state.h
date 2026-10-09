/* Original dragon quest rules, independent of game memory and host services. */
#pragma once

typedef struct { unsigned phase, mask; } dragon_progress;
typedef struct {
    dragon_progress progress;
    int slot, dialog;
    unsigned cooldown, toast;
} dragon_quest_state;
typedef struct { unsigned matched; } dragon_song_state;

static inline int dragon_progress_valid(dragon_progress p) {
    return p.phase <= 4 && p.mask <= 7 &&
           (p.phase < 2 ? p.mask == 0 : p.phase == 2 || p.mask == 7);
}
static inline dragon_progress dragon_progress_normalize(dragon_progress p) {
    if (!dragon_progress_valid(p)) return (dragon_progress){0, 0};
    if (p.phase == 2 && p.mask == 7) p.phase = 3;
    return p;
}
static inline int dragon_space(char c) { return c==' ' || c=='\n' || c=='\r' || c=='\t'; }
static inline int dragon_number(const char* text, unsigned size, unsigned* at, unsigned* value) {
    unsigned n=0, digits=0;
    while (*at<size && dragon_space(text[*at])) ++*at;
    while (*at<size && text[*at]>='0' && text[*at]<='9') {
        unsigned digit=(unsigned)(text[(*at)++]-'0');
        if (n > (0xffffffffu-digit)/10) return 0;
        n=n*10+digit; ++digits;
    }
    *value=n;
    return digits!=0;
}
/* Compatible with the prototype's human-readable progress; reject trailing data. */
static inline int dragon_progress_decode(const char* text, unsigned size, dragon_progress* result) {
    const char magic[]="WWHD_DRAGON_QUEST";
    unsigned at=0, version;
    dragon_progress p;
    for (; at<sizeof(magic)-1; ++at)
        if (at>=size || text[at]!=magic[at]) return 0;
    if (at>=size || !dragon_space(text[at]) ||
        !dragon_number(text,size,&at,&version) || version!=1 ||
        at>=size || !dragon_space(text[at]) ||
        !dragon_number(text,size,&at,&p.phase) ||
        at>=size || !dragon_space(text[at]) ||
        !dragon_number(text,size,&at,&p.mask) || !dragon_progress_valid(p)) return 0;
    while (at<size && dragon_space(text[at])) ++at;
    if (at!=size) return 0;
    *result=dragon_progress_normalize(p);
    return 1;
}
static inline unsigned dragon_progress_encode(dragon_progress p, char output[25]) {
    const char prefix[]="WWHD_DRAGON_QUEST 1\n";
    unsigned i;
    p=dragon_progress_normalize(p);
    for (i=0;i<sizeof(prefix)-1;++i) output[i]=prefix[i];
    output[i++]=(char)('0'+p.phase); output[i++]=' ';
    output[i++]=(char)('0'+p.mask); output[i++]='\n'; output[i]=0;
    return i;
}
static inline void dragon_quest_load(dragon_quest_state* q, int slot, dragon_progress p) {
    q->slot=slot; q->progress=dragon_progress_normalize(p);
    q->dialog=0; q->cooldown=0; q->toast=0;
}
static inline int dragon_quest_invite(dragon_quest_state* q, int leaf, int helped_valoo) {
    if (q->slot<0 || q->slot>2 || q->progress.phase || !leaf || !helped_valoo) return 0;
    q->progress.phase=1; return 1;
}
static inline int dragon_quest_accept(dragon_quest_state* q) {
    if (q->progress.phase!=1) return 0;
    q->progress.phase=2; return 1;
}
static inline int dragon_quest_chime(dragon_quest_state* q, unsigned index) {
    if (q->progress.phase!=2 || index>=3 || (q->progress.mask&(1u<<index))) return 0;
    q->progress.mask|=1u<<index; q->toast=120;
    if (q->progress.mask==7) q->progress.phase=3;
    return 1;
}
static inline int dragon_quest_learn(dragon_quest_state* q) {
    if (q->progress.phase!=3) return 0;
    q->progress=(dragon_progress){4,7}; q->toast=150; return 1;
}
/* Stock song result is unchanged unless a complete eligible custom song matches. */
static inline int dragon_song_beat(dragon_song_state* song, unsigned index,
                            unsigned direction, unsigned meter, int eligible) {
    static const unsigned pattern[6]={1,2,1,4,3,2};
    if (!eligible) { song->matched=0; return 0; }
    if (!index) song->matched=0;
    if (meter==6 && index==song->matched && index<6 && direction==pattern[index])
        ++song->matched;
    else song->matched=0;
    return song->matched==6;
}
