/* SPDX-License-Identifier: MPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include "../src/editor.h"
int main(void) {
    EcEditor e={0}; e.saved=ee_defaults(); assert(ee_valid(&e.saved)); ee_begin(&e);
    assert(ee_touch(&e,2,300,185)==-1); /* opener release cannot save */
    ee_touch(&e,0,-150,-180); ee_touch(&e,1,-400,-240); ee_touch(&e,2,-400,-240);
    assert(e.draft.button[0].x==-400 && e.draft.button[0].y==-240 && e.saved.button[0].x==-150);
    ee_apply(&e,EE_GRID); ee_touch(&e,0,-400,-240); ee_touch(&e,2,113,-137);
    assert(e.draft.button[0].x==120 && e.draft.button[0].y==-140);
    ee_touch(&e,0,120,-140); ee_touch(&e,1,9999,9999); ee_touch(&e,2,9999,9999);
    assert(ee_valid(&e.draft));
    for(int i=0;i<100;++i) ee_apply(&e,EE_LARGER);
    assert(e.draft.button[0].size==180 && ee_valid(&e.draft));
    for(int i=0;i<100;++i) ee_apply(&e,EE_SMALLER);
    assert(e.draft.button[0].size==60 && ee_valid(&e.draft));
    ee_apply(&e,EE_OPACITY); assert(e.draft.button[0].opacity==80);
    for(int i=0;i<4;++i) ee_apply(&e,EE_OPACITY); assert(e.draft.button[0].opacity==100);
    ee_apply(&e,EE_SHAPE); ee_apply(&e,EE_FILL); ee_apply(&e,EE_ICONS); ee_apply(&e,EE_VISIBLE);
    assert(e.draft.button[0].shape==1 && e.draft.button[0].filled && !e.draft.button[0].icons && !e.draft.button[0].visible);
    ee_touch(&e,0,200,305); assert(e.selected==3); /* tabs recover any overlapping/hidden button */
    ee_touch(&e,0,300,185); assert(ee_touch(&e,2,300,185)==EE_SAVE);
    ee_touch(&e,0,300,185); ee_touch(&e,1,350,185); assert(ee_touch(&e,2,300,185)==-1);
    ee_touch(&e,0,300,185); ee_touch(&e,3,300,185); assert(ee_touch(&e,2,300,185)==-1);
    ee_touch(&e,0,300,185); ee_touch(&e,4,2,0); assert(ee_touch(&e,2,300,185)==-1);
    ee_close(&e); ee_begin(&e); assert(e.draft.button[0].x==-150 && e.draft.button[0].visible);
    EcLayoutFile f={0x45434C59,2,5,0,e.saved}; f.checksum=ee_hash(&f); assert(ee_file_valid(&f));
    ++f.layout.button[0].x; assert(!ee_file_valid(&f)); f.checksum=ee_hash(&f); assert(ee_file_valid(&f));
    f.layout.button[0].size=999; f.checksum=ee_hash(&f); assert(!ee_file_valid(&f));
    f.layout=ee_defaults(); f.version=3; f.checksum=ee_hash(&f); assert(!ee_file_valid(&f));
    /* A real v1 four-button file migrates without moving any macro. */
    EcLayoutFileV1 old={.magic=0x45434c59,.version=1,.sequence=7,.grid=1};
    EcLayout defaults=ee_defaults();for(int i=0;i<4;++i)old.button[i]=defaults.button[i];
    unsigned h=2166136261u;
    h=(h^old.magic)*16777619u;h=(h^old.version)*16777619u;h=(h^old.sequence)*16777619u;
    for(int i=0;i<4;++i) {
        unsigned words[]={old.button[i].x,old.button[i].y,100,100,0,0,1,1};
        for(int j=0;j<8;++j)h=(h^words[j])*16777619u;
    }
    old.checksum=(h^1u)*16777619u;
    unsigned char* dst=(unsigned char*)&f;const unsigned char* src=(const unsigned char*)&old;
    for(unsigned i=0;i<sizeof(old);++i)dst[i]=src[i];
    assert(ee_decode(&f,sizeof(old)) && f.version==2 && f.sequence==7 && f.layout.grid);
    for(int i=0;i<4;++i)assert(f.layout.button[i].x==defaults.button[i].x);
    assert(f.layout.button[4].x==348 && !ee_decode(&f,8));
    ee_apply(&e,EE_RESET); assert(ee_valid(&e.draft) && !e.draft.grid);
    puts("Editor: drag, bounds, snapping, actions, cancel and file validation passed");
}
