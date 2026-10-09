/* Renderer-independent HUD commands; all buffers belong to the mod. */
#include "hud.h"
#include "quest.h"
dragon_hud_state dragon_hud;
static wwhd_hud_element element;
static char height_text[80],note_text[80],chime_text[64];
static unsigned text_length(const char* text) { unsigned i=0;while(text[i])++i;return i; }
static char* number(char* out,int n) {
    char reversed[12]; unsigned size=0;
    if(n<0){*out++='-'; n=-(n+1); ++n;}
    do {reversed[size++]=(char)('0'+n%10);n/=10;} while(n);
    while(size)*out++=reversed[--size];return out;
}
static char* append(char* out,const char* text) { while(*text)*out++=*text++;return out; }
static void text(u32 list,const char* value,float y,float size,u32 rgba) {
    if(!value || !*value)return;
    element=(wwhd_hud_element){0};element.kind=WWHD_HUD_TEXT;element.anchor=WWHD_HUD_TOP_LEFT;
    element.x=42;element.y=y;element.size=size;element.rgba=rgba;
    element.text=value;element.text_bytes=text_length(value);wwhd_hud_emit(list,&element);
}
static void draw(u32 list) {
    static const char* titles[]={"VALOO · READY","VALOO · ARRIVING","VALOO · APPROACHING",
        "VALOO · AUTO GRAPPLE","VALOO · LIFTING YOU","VALOO · DRAGON VIEW","VALOO · DEPARTING","CALL OF THE SKY"};
    if(!dragon_hud.visible)return;
    int phase=dragon_hud.phase;
    if(phase<DRAGON_IDLE || phase>DRAGON_SONG)return;
    int quest=dragon_hud.quest_visible && phase==DRAGON_IDLE;
    float h=quest?220:160, y=720-h-24;
    element=(wwhd_hud_element){0};element.kind=WWHD_HUD_RECT;element.anchor=WWHD_HUD_TOP_LEFT;
    element.x=24;element.y=y;element.w=560;element.h=h;element.rgba=0x060e14e0;
    wwhd_hud_emit(list,&element);
    const char *title=titles[phase],*line1="Automatic Grappling Hook pickup, then lift",*line2="",*line3="",*action="Flight controls unlock after the lift";
    if(phase==DRAGON_RIDING) {line1="Left stick ← / → Turn   ↑ / ↓ Climb / dive"; action="A Release · Then press your Deku Leaf button";}
    if(phase==DRAGON_IDLE) {
        line1="Wind Waker · Call of the Sky: ↑ → ↑ ← ↓ →";
        action="L + D-pad Up opens the Wind Waker · Use right stick";
        if(dragon_hud.conducting) {
            char* out=append(note_text,"Hold left stick right for 6 beats · Notes ");
            out=number(out,(int)dragon_hud.notes);out=append(out," / 6");*out=0;action=note_text;
        }
    }
    if(phase==DRAGON_SONG) {line1="Call of the Sky · Valoo hears your call";action="Conducting, then automatic hook pickup";}
    if(quest) {
        title="VALOO’S GRATITUDE";
        unsigned mask=dragon_quest.progress.mask;
        char* out=append(chime_text,mask&1?"↑ →":"? ?");out=append(out,"   ");
        out=append(out,mask&2?"↑ ←":"? ?");out=append(out,"   ");out=append(out,mask&4?"↓ →":"? ?");*out=0;
        switch(dragon_quest.dialog) {
        case 1:title="A LETTER FROM MEDLI";line1="Valoo has not forgotten your kindness.";line2="Come to the high entrance on Dragon Roost.";line3="There is something he wishes to give you.";action="A Close letter · B Close";break;
        case 2:title="MEDLI";line1="Three ancient wind chimes have fallen silent.";line2="Find them by harbor, cliff and offshore lookout.";line3="Strike each with your sword to reveal two notes.";action="A Accept side quest · B Leave";break;
        case 3:title="MEDLI";line1="Listen to the wind chimes around the island.";line2="Climb, grapple and glide to reach all three.";line3=chime_text;action="A Continue · B Close";break;
        case 4:title="MEDLI · CALL OF THE SKY";line1="Those notes are Valoo’s promise to you.";line2="Conduct them here, and he will answer.";line3="↑ → ↑ ← ↓ → · Six beats (left stick →)";action="A Continue, then open your Wind Waker";break;
        case 5:title="MEDLI";line1="If you need his wings, let the wind carry your song.";line2="Valoo will answer your call over the Great Sea.";action="A Continue · B Close";break;
        default:
            switch(dragon_quest.progress.phase) {
            case 0:line1="Help Valoo and obtain the Deku Leaf.";line2="An optional adventure awaits on Dragon Roost.";action="";break;
            case 1:line1="A Rito invitation has arrived.";line2="Medli awaits at Dragon Roost’s high entrance.";action="D-pad Left Read letter · Near Medli: A Talk";break;
            case 2:line1="Restore the harbor, cliff and lookout chimes.";line2=chime_text;action="Strike a chime with your sword · Return to Medli";break;
            case 3:line1="Return to Medli at Dragon Roost’s high entrance.";line2="Play ↑ → ↑ ← ↓ → there to learn the song.";action="Near Medli: A Talk · Wind Waker: six beats";break;
            default:line1="Call of the Sky learned!";action="A Talk to Medli";break;
            }
        }
    }
    text(list,title,y+16,22,0x8cf0d9ff);text(list,line1,y+50,18,0xffffffff);
    if(quest){text(list,line2,y+82,18,0xffffffff);text(list,line3,y+114,18,0xffffffff);text(list,action,y+h-40,16,0xffffffff);}
    else {
        text(list,action,y+86,18,0xffffffff);
        int altitude=dragon_hud.altitude>=0 && dragon_hud.altitude<100000?(int)dragon_hud.altitude:0;
        int minimum=dragon_hud.minimum>=0 && dragon_hud.minimum<100000?(int)dragon_hud.minimum:0;
        char* out=append(height_text,"Altitude ");out=number(out,altitude);out=append(out,"   Flight min ");
        out=number(out,minimum);out=append(out," / max 16000");*out=0;text(list,height_text,y+122,18,0xffffffff);
    }
}
void dragon_hud_register(void) {wwhd_hud_register(draw,WWHD_HUD_TV);}
