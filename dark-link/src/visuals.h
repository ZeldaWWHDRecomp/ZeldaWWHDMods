/* Scoped matte palette: charcoal body, red eyes, black aura. Keep native light directions
 * and the user-confirmed 0.1.5 submission boundaries unchanged. */
#pragma once
#define DL_MAX_MATERIALS 96
#define DL_DRAW_DEPTH 16
#define DL_FIRST_COLOR 4u
#define DL_COLOR_COUNT 7u /* C0..C2 (4..6), K0..K3 (7..10) */
#define DL_TINT_MASK 0x7f0u
#define DL_EYE_COUNT 6u
#define DL_SAVED_MATERIALS (DL_MAX_MATERIALS * DL_MODEL_PARTS)
typedef struct {
    u32* flags;
    u32 mask;
    u32* color[DL_COLOR_COUNT];
    u32 rgb[DL_COLOR_COUNT][3];
} DLMaterialSave;
static DLMaterialSave dl_materials[DL_SAVED_MATERIALS];
static u32 dl_material_count, dl_draw_depth;
static u32 dl_draw_marks[DL_DRAW_DEPTH];

static void dl_restore_colors(u32 mark) {
    while (dl_material_count > mark) {
        DLMaterialSave* saved = &dl_materials[--dl_material_count];
        for (u32 i = 0; i < DL_COLOR_COUNT; ++i)
            if (saved->color[i])
                for (u32 j = 0; j < 3; ++j) saved->color[i][j] = saved->rgb[i][j];
        /* These bits select overrides; do NOT leave newly enabled bits set.
         * Keep changes made by native code to every bit we do not own. */
        *saved->flags = (*saved->flags & ~DL_TINT_MASK) | saved->mask;
    }
}
static void dl_palette(u8* material, u32 surface) {
    u32* flags = (u32*)(material + 0xa0);
    DLMaterialSave* saved = 0;
    for (u32 i = 0; i < dl_material_count; ++i)
        if (dl_materials[i].flags == flags) { saved = &dl_materials[i]; break; }
    if (!saved) {
        if (dl_material_count >= DL_SAVED_MATERIALS) return;
        saved = &dl_materials[dl_material_count++];
        saved->flags = flags; saved->mask = *flags & DL_TINT_MASK;
        for (u32 i = 0; i < DL_COLOR_COUNT; ++i) {
            saved->color[i] = (u32*)dl_material_parameter(flags, DL_FIRST_COLOR + i);
            if (saved->color[i])
                for (u32 j = 0; j < 3; ++j) saved->rgb[i][j] = saved->color[i][j];
        }
    }
    for (u32 i = 0; i < DL_COLOR_COUNT; ++i) {
        f32* color = (f32*)saved->color[i];
        if (!color) continue;
        if (surface == DL_SURFACE_BODY) {
            /* The native environment path supplies ambient C0 (slot 4) and
             * diffuse K0 (slot 7): d_kankyo_6 setLight_actor / setLightTevColorType.
             * Give them distinct dark, cool tones instead of flattening both
             * to zero. Other colour registers retain a dark neutral base.
             * Values are linear shader inputs, not 8-bit display colours.
             * Leave native normals, toon ramp and light directions untouched. */
            static const f32 shadow[3] = {0.006f, 0.009f, 0.014f};
            static const f32 diffuse[3] = {0.035f, 0.040f, 0.050f};
            static const f32 base[3] = {0.018f, 0.021f, 0.026f};
            const f32* tint = i == 0 ? shadow : i == 3 ? diffuse : base;
            for (u32 c=0;c<3;++c) color[c] = tint[c];
        } else {
            color[0] = surface == DL_SURFACE_EYE ? 1.0f : 0.0f;
            color[1] = 0.0f;
            color[2] = 0.0f;
        }
        *flags |= 1u << (DL_FIRST_COLOR + i);
    }
    /* No alpha writes, no MAT/AMB channel overrides, no fog changes. */
}
static void dl_find_eyes(u8* data, u32 count, s32 eyes[DL_EYE_COUNT]) {
    /* All eye variants from daPy_lk_c::playerInit (02410BE8). Eyebrows
     * (mayu*) are deliberately body-colored; blinking/alpha remain native. */
    static const char* const names[DL_EYE_COUNT] = {
        "eyeL", "eyeR", "eyeLdamA", "eyeRdamA", "eyeLdamB", "eyeRdamB"
    };
    u8* resource = (u8*)dl_word(data, 0);
    if (!resource) return;
    u32 relative = dl_word(resource, 0x18);
    if (!relative) return;
    void* table = resource + 0x18 + (s32)relative;
    for (u32 i = 0; i < DL_EYE_COUNT; ++i) {
        s32 index = dl_material_name_index(table, names[i]);
        if (index >= 0 && (u32)index < count) eyes[i] = index;
    }
}
static void dl_visual_begin(void* model) {
    u32 depth = dl_draw_depth++;
    if (depth >= DL_DRAW_DEPTH) return;
    dl_draw_marks[depth] = dl_material_count;
    if (!dl_state.active || !dl_enabled || !model) return;
    daPy_lk_c* link = dl_player();
    if (!link || link != dl_owner || (dl_word(link, 0x3bc) & 0x800)) return;
    u32 address = (u32)model;
    int part = dl_model_part(link, address);
    if (part < 0) return;
    if (dl_diagnostics && address == link->mpCLModel) dl_probe_increment(&dl_probe.draws);
    u8* data = (u8*)dl_word(model, 0xac);
    if (!data) return;
    u32 count = dl_word(data, 0x0c);
    u8* materials = (u8*)dl_word(data, 0x10);
    if (!materials || !count || count > DL_MAX_MATERIALS) return;
    s32 eyes[DL_EYE_COUNT];
    for (u32 i = 0; i < DL_EYE_COUNT; ++i) eyes[i] = -1;
    if (address == link->mpCLModel) dl_find_eyes(data, count, eyes);
    for (u32 i = 0; i < count; ++i) {
        u8* material = materials + i * 0x39c;
        u32 animation = dl_word(material, 0x24);
        int eye = 0;
        if (address == link->mpCLModel) {
            for (u32 e = 0; e < DL_EYE_COUNT; ++e)
                if ((s32)i == eyes[e]) eye = 1;
            if (animation && (animation == link->m_tex_eye_scroll[0] || animation == link->m_tex_eye_scroll[1])) eye = 1;
        }
        dl_palette(material, part == 7 ? DL_SURFACE_AURA : eye ? DL_SURFACE_EYE : DL_SURFACE_BODY);
    }
}
static void dl_visual_end(void) {
    if (!dl_draw_depth) return;
    u32 depth = --dl_draw_depth;
    if (depth < DL_DRAW_DEPTH) dl_restore_colors(dl_draw_marks[depth]);
}
/* Restore all four hook pairs from 0.1.0. The mDoExt wrappers encompass BOTH
 * model preparation and viewCalc/submission. Restoring at the inner native
 * entry return (0.1.4) removed the tint before that subsequent stage.
 * viewCalc and joint entry are submission boundaries, not immediate GPU draws;
 * preserve these user-confirmed boundaries when extending the palette.
 * Host tests do not validate the asynchronous renderer or shader output. */
WWHD_HOOK(DL_MODEL_ENTRY, dl_entry_before, (void* model)) { dl_visual_begin(model); }
WWHD_HOOK_RETURN(DL_MODEL_ENTRY, dl_entry_after, (void* model)) { (void)model; dl_visual_end(); }
WWHD_HOOK(DL_MODEL_UPDATE, dl_update_before, (void* model, void* data)) { (void)data; dl_visual_begin(model); }
WWHD_HOOK_RETURN(DL_MODEL_UPDATE, dl_update_after, (void* model, void* data)) { (void)data; (void)model; dl_visual_end(); }
WWHD_HOOK(DL_MODEL_VIEW, dl_view_before, (void* model)) { dl_visual_begin(model); }
WWHD_HOOK_RETURN(DL_MODEL_VIEW, dl_view_after, (void* model)) { (void)model; dl_visual_end(); }
WWHD_HOOK(DL_JOINT_ENTRY, dl_joint_before, (void* model, void* joint)) { (void)joint; dl_visual_begin(model); }
WWHD_HOOK_RETURN(DL_JOINT_ENTRY, dl_joint_after, (void* model, void* joint)) { (void)model; (void)joint; dl_visual_end(); }
