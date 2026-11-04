/* Authored pane visibility is animated independently of the progress curve.
 * Keep the hierarchy so hidden flash parents cannot become opaque overlays.
 * Layout/track formats: tools/story_mods/lyt/{sections,anim}. */
#pragma once
#define DL_MAGIC_STYLES 128
static struct { char name[24]; u32 parent,visible,alpha,flash; } dl_magic_styles[DL_MAGIC_STYLES];
static u32 dl_magic_style_count;
static s32 dl_magic_style_find(const char* name) {
    for(u32 i=0;i<dl_magic_style_count;++i)
        if(dl_str_equal(name,dl_magic_styles[i].name,24)) return (s32)i;
    return -1;
}
static int dl_magic_style_parse(u32 f,u32 size) {
    u32 stack[32],depth=0,parent=DL_MAGIC_STYLES,last=DL_MAGIC_STYLES;
    u32 at=f+*(u16*)(f+6),count=*(u16*)(f+16);
    dl_magic_style_count=0;
    for(u32 i=0;i<count;++i) {
        if(!dl_blob_span(f,size,at,8)) return 0;
        u32 n=dl_word((void*)at,4),type=*(u32*)at;
        if(n<8 || !dl_blob_span(f,size,at,n)) return 0;
        if(type==0x70617331) { /* pas1 */
            if(depth==32 || last==DL_MAGIC_STYLES) return 0;
            stack[depth++]=parent;parent=last;
        } else if(type==0x70616531) { /* pae1 */
            if(!depth) return 0;
            parent=stack[--depth];last=parent;
        } else if(type==0x70616e31 || type==0x70696331 || type==0x776e6431 ||
                  type==0x74787431 || type==0x70727431 || type==0x626e6431) {
            if(n<0x54 || dl_magic_style_count==DL_MAGIC_STYLES) return 0;
            last=dl_magic_style_count++;
            if(!dl_copy_string(dl_magic_styles[last].name,24,(char*)(at+12),24)) return 0;
            dl_magic_styles[last].parent=parent;
            dl_magic_styles[last].visible=*((u8*)at+8)&1;
            dl_magic_styles[last].alpha=*((u8*)at+10);
            dl_magic_styles[last].flash=0;
        }
        at+=n;
    }
    if(depth) return 0;
    /* Native Flash animation is bound to G_Flash_00. Never substitute a
     * warning flash for the permanent gauge frame, even without its animation. */
    at=f+*(u16*)(f+6);
    for(u32 i=0;i<count;++i) {
        u32 n=dl_word((void*)at,4);
        if(*(u32*)at==0x67727031 && n>=36 && dl_str_equal((char*)(at+8),"G_Flash_00",24)) {
            u32 members=*(u16*)(at+32);
            if(members>(n-36)/24) return 0;
            for(u32 j=0;j<members;++j) {
                s32 p=dl_magic_style_find((char*)(at+36+j*24));
                if(p>=0) dl_magic_styles[p].flash=1;
            }
        }
        at+=n;
    }
    return 1;
}
static u32 dl_magic_style_alpha(const char* name) {
    s32 p=dl_magic_style_find(name);u32 alpha=255;
    if(p<0) return 0;
    for(u32 n=0;n<DL_MAGIC_STYLES && (u32)p<dl_magic_style_count;++n) {
        if(!dl_magic_styles[p].visible || dl_magic_styles[p].flash) return 0;
        alpha=alpha*dl_magic_styles[p].alpha/255;
        p=(s32)dl_magic_styles[p].parent;
    }
    return alpha;
}
/* The selected native gauge frame is independently visible, like base/fill.
 * Animation group membership is not proof that its frame art is a flash-only
 * overlay. We copy only the border slices at the non-warning material state;
 * native window content and unrelated flash panes remain excluded. */
static u32 dl_magic_frame_alpha(const char* name,u32 variant) {
    const char* wanted=variant?"W_MagicBase_01":"W_MagicBase_00";
    if(dl_str_equal(name,wanted,24)) return 255;
    return dl_magic_style_alpha(name);
}
