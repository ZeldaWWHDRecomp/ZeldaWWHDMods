/* Reconstruct the native displayed, non-warning style before progress. Native
 * bindings/state: hd_screen_magic_bar.cpp at decomp b2811a3ce838d1f875bc8e68536c1950af5d7ffc.
 * InAll -> end, Flash -> 0, ColorChange -> 0 (ordinary Link), ScaleChange -> 0;
 * then the full 16/32-frame G_Bar_00 curve. Never mutate the live HUD/save. */
#pragma once
static u32 dl_magic_animation_ok[2];
static struct { u32 file,bytes,pai; f32 frame; } dl_magic_animations[5];
static int dl_magic_number(f32 v) { return v>=-1000000 && v<=1000000; }
static int dl_magic_key_value(u32 f,u32 bytes,u32 e,f32 frame,f32* result) {
    if((e&3)||!dl_blob_span(f,bytes,e,12)) return 0;
    u32 curve=*((u8*)e+2),count=*(u16*)(e+4),offset=dl_word((void*)e,8);
    if(offset<12||offset>bytes) return 0;
    u32 keys=e+offset;
    u32 stride=curve==1?8:curve==2?12:0;
    if(!stride||!count||count>256||(keys&3)||!dl_blob_span(f,bytes,keys,count*stride)) return 0;
    f32 previous=-1000001;
    for(u32 k=0;k<count;++k) {
        u32 key=keys+k*stride;f32 t=*(f32*)key;
        if(!dl_magic_number(t)||t<previous) return 0;
        previous=t;
        if(curve==2 && (!dl_magic_number(*(f32*)(key+4))||!dl_magic_number(*(f32*)(key+8)))) return 0;
    }
    u32 left=0;
    while(left+1<count && *(f32*)(keys+(left+1)*stride)<=frame) ++left;
    u32 a=keys+left*stride;
    *result=curve==1?(f32)*(u16*)(a+4):*(f32*)(a+4);
    if(curve==1 || left+1==count || frame<=*(f32*)a) return 1;
    u32 b=a+stride;f32 dt=*(f32*)b-*(f32*)a;
    if(!(dt>0)) return 1;
    f32 t=(frame-*(f32*)a)/dt,t2=t*t,t3=t2*t;
    *result=(2*t3-3*t2+1)**(f32*)(a+4)+(t3-2*t2+t)*dt**(f32*)(a+8)
        +(-2*t3+3*t2)**(f32*)(b+4)+(t3-t2)*dt**(f32*)(b+8);
    return dl_magic_number(*result);
}
static u32 dl_magic_colour_byte(u32 c,u32 channel,f32 value) {
    u32 v=value<0?0:value>255?255:(u32)(value+0.5f),shift=24-channel*8;
    return (c&~(255u<<shift))|(v<<shift);
}
static int dl_magic_tracks(u32 f,u32 bytes,u32 pai,f32 frame,int apply) {
    if(!dl_blob_span(f,bytes,pai,20)) return 0;
    u32 count=*(u16*)(pai+14),table=pai+dl_word((void*)pai,16);
    if(count>256 || !dl_blob_span(f,bytes,table,count*4)) return 0;
    for(u32 i=0;i<count;++i) {
        u32 c=pai+dl_word((void*)table,i*4);
        if(!dl_blob_span(f,bytes,c,32)) return 0;
        u32 tags=*((u8*)c+28),kind=*((u8*)c+29);
        if(tags>32 || !dl_blob_span(f,bytes,c+32,tags*4)) return 0;
        for(u32 j=0;j<tags;++j) {
            u32 tag=c+dl_word((void*)c,32+j*4);
            if(!dl_blob_span(f,bytes,tag,8)) return 0;
            u32 type=*(u32*)tag,entries=*((u8*)tag+4);
            if(!dl_blob_span(f,bytes,tag+8,entries*4)) return 0;
            for(u32 k=0;k<entries;++k) {
                u32 e=tag+dl_word((void*)tag,8+k*4);f32 value;
                if(!dl_magic_key_value(f,bytes,e,frame,&value)) return 0;
                if(!apply) continue;
                u32 index=*(u8*)e,target=*((u8*)e+1);
                if(kind==0 && !dl_magic_material_target) {
                    s32 style=dl_magic_style_find((const char*)c);
                    if(style>=0) {
                        if(type==0x464c5643 && target==16)
                            dl_magic_styles[style].alpha=value<0?0:value>255?255:(u32)(value+0.5f);
                        if(type==0x464c5649 && target==0) dl_magic_styles[style].visible=value!=0;
                    }
                }
                u32 pictures=dl_magic_material_target?1:dl_magic_picture_count;
                for(u32 p=0;p<pictures;++p) {
                    dl_magic_picture* pic=dl_magic_material_target?dl_magic_material_target:&dl_magic_pictures[p];
                    const char* name=kind==1?pic->material:pic->name;
                    if(!dl_str_equal(name,(const char*)c,28)) continue;
                    if(!dl_magic_material_target && pai==dl_magic_animations[4].pai &&
                       ((kind==0 && type==0x464c5041 && (target==6 || target==8)) ||
                        (kind==1 && type==0x464c5453 && index==0 && (target==0 || target==3)))) {
                        f32 initial;
                        if(dl_magic_key_value(f,bytes,e,0,&initial) && dl_abs(initial-value)>0.0001f) pic->progress=1;
                    }
                    if(kind==1 && type==0x464c4d43 && target<8) { /* FLMC */
                        u32* colour=target<4?&pic->black:&pic->white;
                        *colour=dl_magic_colour_byte(*colour,target&3,value);
                    } else if(kind==1 && type==0x464c5453 && index==0 && target<5) /* FLTS */
                        pic->tex_srt[target]=value;
                    else if(kind==0 && type==0x464c5041 && target<10) /* FLPA */
                        pic->pane[target]=value;
                    else if(kind==0 && type==0x464c5643 && target<16) { /* FLVC */
                        u32* colour=&pic->vertex[target/4];
                        *colour=dl_magic_colour_byte(*colour,target&3,value);
                    }
                }
            }
        }
    }
    return 1;
}
/* Select by the authored tag AND binding group, never a filename substring.
 * Store only wholly validated animations; malformed tracks cannot half-apply. */
static void dl_magic_animation_file(u32 f,u32 bytes,u32 variant) {
    if(bytes<20 || *(u32*)f!=0x464c414e || *(u16*)(f+4)!=0xfeff) return;
    u32 at=f+*(u16*)(f+6),sections=*(u16*)(f+16),pat=0,pai=0;
    if(sections>64) return;
    for(u32 i=0;i<sections;++i) {
        if(!dl_blob_span(f,bytes,at,8)) return;
        u32 n=dl_word((void*)at,4);
        if(n<8||(n&3)||!dl_blob_span(f,bytes,at,n)) return;
        if(*(u32*)at==0x70617431) pat=at;
        if(*(u32*)at==0x70616931) pai=at;
        at+=n;
    }
    if(!pat||!pai||!dl_blob_span(f,bytes,pat,28)||!dl_blob_span(f,bytes,pai,20)) return;
    u32 groups=*(u16*)(pat+10),table=pat+dl_word((void*)pat,16);
    u32 name=pat+dl_word((void*)pat,12),duration=*(u16*)(pai+8);
    if(groups>32 || !dl_blob_span(f,bytes,table,groups*28) || !dl_blob_span(f,bytes,name,1)) return;
    char tag[32];u32 length=0;
    while(length<sizeof tag-1 && dl_blob_span(f,bytes,name+length,1) && *((u8*)name+length)) ++length;
    if(length==sizeof tag-1 || !dl_blob_span(f,bytes,name+length,1)) return;
    if(!dl_copy_string(tag,sizeof tag,(char*)name,length+1)) return;
    static const char* const tags[4]={"InAll","Flash","ColorChange","ScaleChange"};
    static const char* const bindings[4]={"G_All_00","G_Flash_00","G_Change_00","G_All_00"};
    s32 slot=-1;f32 frame=0;
    for(u32 i=0;i<groups;++i) {
        const char* group=(const char*)(table+i*28);
        for(u32 j=0;j<4;++j) if(dl_str_equal(tag,tags[j],32) && dl_str_equal(group,bindings[j],24)) {
            slot=(s32)j;frame=j==0?(f32)duration:0;
        }
        if(duration==(variant?32u:16u) && dl_str_equal(group,"G_Bar_00",24)) { slot=4;frame=(f32)duration; }
    }
    if(slot<0 || dl_magic_animations[slot].file || !dl_magic_tracks(f,bytes,pai,frame,0)) return;
    dl_magic_animations[slot].file=f;dl_magic_animations[slot].bytes=bytes;
    dl_magic_animations[slot].pai=pai;dl_magic_animations[slot].frame=frame;
}
static void dl_magic_apply_animations(void) {
    for(u32 i=0;i<5;++i) if(dl_magic_animations[i].file)
        dl_magic_tracks(dl_magic_animations[i].file,dl_magic_animations[i].bytes,
                        dl_magic_animations[i].pai,dl_magic_animations[i].frame,1);
}
static void dl_magic_full_animation(u32 variant) {
    dl_magic_animation_ok[variant]=0;
    memset(dl_magic_animations,0,sizeof dl_magic_animations);
    u32 arc=dl_magic_archive;
    /* The same archive was validated by dl_magic_file immediately above. */
    u32 size=dl_word((void*)arc,8),data=dl_word((void*)arc,12),fat=arc+*(u16*)(arc+4);
    u32 n=*(u16*)(fat+6),nodes=fat+*(u16*)(fat+4);
    for(u32 i=0;i<n;++i) {
        u32 start=dl_word((void*)nodes,i*16+8),end=dl_word((void*)nodes,i*16+12);
        if(end<=start||end>size-data||end-start<20) continue;
        u32 f=arc+data+start;
        if((f&3)||*(u32*)f!=0x464c414e) continue;
        dl_magic_animation_file(f,end-start,variant);
    }
    dl_magic_apply_animations();
    dl_magic_animation_ok[variant]=dl_magic_animations[4].file!=0;
}
