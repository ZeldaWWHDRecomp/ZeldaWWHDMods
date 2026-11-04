/* Per-instance albedo views, applied AFTER native texture animation/refresh.
 * Evidence: story_prop_textures.cpp / story_props.cpp (NNS material bindings,
 * native 027F50F8), d_a_obj_ftree.cpp (J3D -> NNS material index), export_glb.py
 * (_a0 albedo sampler), and Cemu GX2_Texture / LatteReg (RGB component selectors).
 * No image data is copied or changed. Published descriptors are immutable. */
#pragma once
#define DL_TEXTURE_REFRESH 0x027F50F8
#define DL_TEXTURE_BYTES 0xa0u
#define DL_TEXTURE_CACHE 512u
#define DL_TEXTURE_BINDINGS (DL_MAX_MATERIALS * DL_MODEL_PARTS)
typedef struct {
    u32 original[DL_TEXTURE_BYTES / 4];
    u32 view[DL_TEXTURE_BYTES / 4];
    u32 eye;
} DLTextureView;
typedef struct {
    u32 model, materials, index, resource, refs, slot, original, view, eye;
} DLTextureBinding;
static DLTextureView dl_texture_cache[DL_TEXTURE_CACHE];
static u32 dl_texture_count, dl_binding_count;
static DLTextureBinding dl_bindings[DL_TEXTURE_BINDINGS];

static int dl_game_span(u32 p, u32 size) {
    return !(p & 3u) && p >= 0x10000000u && p < 0x50000000u && size <= 0x50000000u - p;
}
static u32 dl_relative(u32 p) {
    if (!dl_game_span(p, 4)) return 0;
    u32 delta = *(u32*)p;
    if (!delta) return 0;
    long long address = (long long)p + (s32)delta;
    return address >= 0x10000000ll && address < 0x50000000ll ? (u32)address : 0;
}
static int dl_texture_model(u32 model) {
    daPy_lk_c* link = dl_player();
    return link && link == dl_owner && dl_actor_id(link) == dl_owner_id &&
        dl_model_part(link, model) >= 0;
}
static int dl_binding_live(const DLTextureBinding* b) {
    if (!dl_texture_model(b->model) || !dl_game_span(b->model, 0xb0) ||
        dl_word((void*)b->model, 0x34) != b->materials) return 0;
    u32 count = *(u16*)(b->model + 0x2a);
    if (b->index >= count || count > DL_MAX_MATERIALS ||
        !dl_game_span(b->materials, count * 0x3c)) return 0;
    u8* material = (u8*)(b->materials + b->index * 0x3c);
    return dl_word(material, 0) == b->resource && dl_word(material, 0x2c) == b->refs &&
        dl_game_span(b->slot, 4);
}
static void dl_texture_restore_model(u32 model) {
    for (u32 i = 0; i < dl_binding_count;) {
        DLTextureBinding* b = &dl_bindings[i];
        if (model && b->model != model) { ++i; continue; }
        /* Native animation or another mod may already have changed this slot.
         * Restore only a still-live binding that contains OUR exact view. */
        if (dl_binding_live(b) && *(u32*)b->slot == b->view) *(u32*)b->slot = b->original;
        dl_bindings[i] = dl_bindings[--dl_binding_count];
    }
}
static void dl_texture_restore_all(void) { dl_texture_restore_model(0); dl_aura_restore(); }

static u32 dl_texture_view(u32 texture, int eye) {
    /* Never guess descriptor layouts or dereference an unrecognized mod view. */
    if (!dl_game_span(texture, DL_TEXTURE_BYTES) || *(u32*)texture != 0x46544558u) return 0;
    const u32* source = (const u32*)texture;
    if (!source[2] || !source[3] || !source[0x28 / 4] || (source[0x88 / 4] & 0xffu) > 5) return 0;
    for (u32 i = 0; i < dl_texture_count; ++i) {
        DLTextureView* cached = &dl_texture_cache[i];
        if (cached->eye != (u32)eye) continue;
        u32 j = 0;
        while (j < DL_TEXTURE_BYTES / 4 && cached->original[j] == source[j]) ++j;
        if (j == DL_TEXTURE_BYTES / 4) return (u32)cached->view;
    }
    if (dl_texture_count == DL_TEXTURE_CACHE) return 0;
    DLTextureView* cached = &dl_texture_cache[dl_texture_count++];
    cached->eye = (u32)eye;
    for (u32 j = 0; j < DL_TEXTURE_BYTES / 4; ++j) cached->view[j] = cached->original[j] = source[j];
    /* GX2Texture starts at FTEX+4. Selectors 4/5 mean constant zero/one.
     * Change both compSel (+0x84) and its precomputed SQ_TEX_RESOURCE_WORD4
     * (+0x90). Keep the alpha selector and ALL other register bits intact. */
    cached->view[0x88 / 4] = (source[0x88 / 4] & 0xffu) | (eye ? 0x05040400u : 0x04040400u);
    cached->view[0x94 / 4] = (source[0x94 / 4] & ~(0x1ffu << 16)) |
        ((eye ? 5u : 4u) << 16) | (4u << 19) | (4u << 22);
    return (u32)cached->view;
}
/* Wii U ResSampler (24 bytes): GX2 words +0x00..08, handle +0x0c,
 * self-relative name +0x10, index +0x14. The handle is NOT a name pointer.
 * Layout checked against BfresLibrary/Shared/Models/Material/Sampler.cs.
 * 0.1.7/8 mistakenly read +0x0c, skipping every material in the user's video. */
static s32 dl_albedo_slot(u32 resource) {
    if (!dl_game_span(resource, 0x34) || *(u32*)resource != 0x464d4154u) return -DL_PROBE_FMAT;
    const u8* r = (const u8*)resource;
    u32 count = r[0x10];
    if (!count || count > 32 || count != r[0x11]) return -DL_PROBE_SAMPLERS;
    u32 samplers = dl_relative(resource + 0x2c);
    if (!dl_game_span(samplers, count * 24)) return -DL_PROBE_SAMPLERS;
    for (u32 i = 0; i < count; ++i) {
        u32 name = dl_relative(samplers + i * 24 + 0x10);
        /* Names need not be word-aligned; bound the four-byte string. */
        if (name && name <= 0x50000000u - 4) {
            const char* s = (const char*)name;
            if (s[0] == '_' && s[1] == 'a' && s[2] == '0' && !s[3]) return (s32)i;
        }
    }
    /* Do not recolor toon ramps, normal maps or a guessed first sampler. */
    return -DL_PROBE_NO_ALBEDO;
}
/* Observe at most three sampler names from a validated material. Truncation and
 * nonprintable bytes are explicit; diagnostics never follow pixel pointers. */
static void dl_probe_samplers(u32 resource, char out[48]) {
    if (out[0] || !dl_game_span(resource, 0x34) || *(u32*)resource != 0x464d4154u) return;
    u32 count = ((u8*)resource)[0x10], n = 0;
    u32 samplers = dl_relative(resource + 0x2c);
    if (!count || count > 32 || !dl_game_span(samplers, count * 24)) return;
    for (u32 i = 0; i < count && i < 3; ++i) {
        if (i) out[n++] = ',';
        u32 name = dl_relative(samplers + i * 24 + 0x10);
        if (!name || name > 0x50000000u - 13) { out[n++] = '?'; continue; }
        u32 j = 0;
        for (; j < 12 && ((u8*)name)[j]; ++j) {
            u8 c = ((u8*)name)[j];
            out[n++] = c >= 32 && c < 127 ? (char)c : '?';
        }
        if (j == 12 && ((u8*)name)[j]) out[n++] = '~';
    }
    if (count > 3) out[n++] = '+';
    out[n] = 0;
}
static void dl_texture_apply(void* model) {
    if (!dl_enabled || !dl_state.active || !dl_texture_model((u32)model)) return;
    int body = (u32)model == dl_owner->mpCLModel;
    int probe = dl_diagnostics && body;
    int part = dl_model_part(dl_owner, (u32)model);
    if (dl_diagnostics) dl_probe_increment(&dl_probe.refresh[part < 3 ? part : 3]);
    if (probe) {
        dl_probe.materials = dl_probe.native_materials = dl_probe.albedo = dl_probe.views = dl_probe.eyes = dl_probe.eye_materials = 0;
        dl_probe.reason = DL_PROBE_OK; dl_probe.skipped = dl_probe.kept = dl_probe.lost = dl_probe.rebound = 0;
        dl_probe.body_samplers[0] = dl_probe.eye_samplers[0] = 0;
        dl_probe.late_calls = dl_probe.late_changed = dl_probe.late_stale = dl_probe.late_rejected = 0;
        dl_probe.late_pointer = dl_probe.late_word = 0;
    }
    if (dl_word(dl_owner, 0x3bc) & 0x800) { if (probe) dl_probe_skip(DL_PROBE_FROZEN); return; }
    u8* data = (u8*)dl_word(model, 0xac);
    if (!dl_game_span((u32)data, 0x14)) { if (probe) dl_probe_skip(DL_PROBE_MODEL_DATA); return; }
    u32 count = dl_word(data, 0xc), materials = dl_word(data, 0x10);
    u32 native_count = *(u16*)((u8*)model + 0x2a), native_materials = dl_word(model, 0x34);
    if (probe) { dl_probe.materials = count; dl_probe.native_materials = native_count; }
    if (!count || count > DL_MAX_MATERIALS || !native_count || native_count > DL_MAX_MATERIALS ||
        !dl_game_span(materials, count * 0x39c) || !dl_game_span(native_materials, native_count * 0x3c)) {
        if (probe) dl_probe_skip(DL_PROBE_MATERIALS);
        return;
    }
    s32 eyes[DL_EYE_COUNT];
    for (u32 e = 0; e < DL_EYE_COUNT; ++e) eyes[e] = -1;
    if (body) dl_find_eyes(data, count, eyes);
    for (u32 i = 0; i < count; ++i) {
        if (dl_binding_count == DL_TEXTURE_BINDINGS) { if (probe) dl_probe_skip(DL_PROBE_CAPACITY); break; }
        u8* material = (u8*)(materials + i * 0x39c);
        u32 resource = dl_word(material, 0);
        int eye = 0;
        if (body) {
            for (u32 e = 0; e < DL_EYE_COUNT; ++e) if ((s32)i == eyes[e]) eye = 1;
            u32 animation = dl_word(material, 0x24);
            if (animation && (animation == dl_owner->m_tex_eye_scroll[0] || animation == dl_owner->m_tex_eye_scroll[1])) eye = 1;
        }
        if (probe) {
            dl_probe.eye_materials += (u32)eye;
            dl_probe_samplers(resource, eye ? dl_probe.eye_samplers : dl_probe.body_samplers);
        }
        s32 sampler = dl_albedo_slot(resource);
        if (sampler < 0) { if (probe) dl_probe_skip((u32)-sampler); continue; }
        if (probe) ++dl_probe.albedo;
        u32 index = *(u16*)(resource + 0xc);
        if (index >= native_count) { if (probe) dl_probe_skip(DL_PROBE_INDEX); continue; }
        u8* native = (u8*)(native_materials + index * 0x3c);
        if (dl_word(native, 0) != resource) { if (probe) dl_probe_skip(DL_PROBE_RESOURCE); continue; }
        u32 refs = dl_word(native, 0x2c), slots = ((u8*)resource)[0x11];
        if (!dl_game_span(refs, slots * 4)) { if (probe) dl_probe_skip(DL_PROBE_REFS); continue; }
        u32 slot = refs + (u32)sampler * 4;
        int duplicate = 0;
        for (u32 n = 0; n < dl_binding_count; ++n) if (dl_bindings[n].slot == slot) duplicate = 1;
        if (duplicate) { if (probe) dl_probe_skip(DL_PROBE_DUPLICATE); continue; }
        u32 original = *(u32*)slot, view = dl_texture_view(original, eye);
        if (!view) { if (probe) dl_probe_skip(dl_texture_count == DL_TEXTURE_CACHE ? DL_PROBE_CAPACITY : DL_PROBE_TEXTURE); continue; }
        dl_bindings[dl_binding_count++] = (DLTextureBinding){(u32)model, native_materials, index, resource, refs, slot, original, view, (u32)eye};
        *(u32*)slot = view;
        if (probe) { ++dl_probe.views; dl_probe.eyes += (u32)eye; }
    }
}
/* viewCalc itself calls the base texture refresh (story_props.cpp documents
 * this nesting). Replacements after that refresh must be repaired at viewCalc
 * RETURN, not entry. Separate eye-joint entries retain their existing check.
 * Never rewrite published descriptors or snapshot our view as an original.
 * Cancellation restores the latest supported native animation frame. */
static void dl_texture_resubmit(void* model) {
    if (!dl_enabled || !dl_state.active || !dl_texture_model((u32)model) ||
        (dl_word(dl_owner, 0x3bc) & 0x800)) return;
    int probe = dl_diagnostics && (u32)model == dl_owner->mpCLModel;
    if (probe) dl_probe_increment(&dl_probe.late_calls);
    for (u32 i = 0; i < dl_binding_count; ++i) {
        DLTextureBinding* b = &dl_bindings[i];
        if (b->model != (u32)model) continue;
        if (!dl_binding_live(b)) { if (probe) dl_probe_increment(&dl_probe.late_stale); continue; }
        u32 current = *(u32*)b->slot;
        if (current == b->view) continue;
        if (probe) dl_probe_increment(&dl_probe.late_changed);
        u32 view = dl_texture_view(current, (int)b->eye);
        if (!view) {
            if (probe) {
                if (!dl_probe.late_rejected) {
                    dl_probe.late_pointer = current;
                    dl_probe.late_word = dl_game_span(current, 4) ? *(u32*)current : 0;
                }
                dl_probe_increment(&dl_probe.late_rejected);
            }
            continue;
        }
        b->original = current;
        b->view = view;
        *(u32*)b->slot = view;
        if (probe) dl_probe_increment(&dl_probe.rebound);
    }
}
/* The runtime's native prop texture path uses this exact synchronous refresh
 * boundary. Bindings must survive the outer wrapper return: queued packets read
 * per-instance material references later. Restore before the next refresh so
 * texture animation always starts from its own live texture, never our view. */
WWHD_HOOK(DL_TEXTURE_REFRESH, dl_texture_before, (void* model)) {
    if (model && dl_texture_model((u32)model)) dl_texture_restore_model((u32)model);
}
WWHD_HOOK_RETURN(DL_TEXTURE_REFRESH, dl_texture_after, (void* model)) {
    dl_texture_apply(model);
}

/* A retained binding is evidence about CPU submission only, not shader output. */
static void dl_diag_submission(void* model) {
    if (!dl_state.active || !dl_enabled || !dl_texture_model((u32)model) || (u32)model != dl_owner->mpCLModel) return;
    dl_probe.kept = dl_probe.lost = 0;
    for (u32 i = 0; i < dl_binding_count; ++i) {
        const DLTextureBinding* b = &dl_bindings[i];
        if (b->model != (u32)model) continue;
        if (dl_binding_live(b) && *(u32*)b->slot == b->view) ++dl_probe.kept;
        else ++dl_probe.lost;
    }
}
static void dl_diag_hud(u32 list) {
    static const char* const reasons[] = {"OK", "NO_REFRESH", "FROZEN", "MODEL_DATA",
        "MATERIALS", "FMAT", "SAMPLERS", "NO_ALBEDO", "INDEX", "RESOURCE", "REFS", "TEXTURE", "CAPACITY", "DUPLICATE"};
    u32 lengths[5], n = 0;
#define DL_S(s) n = dl_probe_append(line, n, s)
#define DL_N(v) n = dl_probe_number(line, n, v)
    char* line = dl_probe_lines[0];
    DL_S("DL 0.1.36 "); DL_S(dl_state.active ? "ON" : "OFF (last sample)");
    DL_S("  Draw="); DL_N(dl_probe.draws); DL_S("  Refresh B/H/Hair/Gear=");
    DL_N(dl_probe.refresh[0]); DL_S("/"); DL_N(dl_probe.refresh[1]); DL_S("/"); DL_N(dl_probe.refresh[2]); DL_S("/"); DL_N(dl_probe.refresh[3]);
    DL_S(" Magic="); DL_N((dl_magic_images[0][0]!=0)+(dl_magic_images[0][1]!=0)+(dl_magic_images[1][0]!=0)+(dl_magic_images[1][1]!=0));
    DL_S(" Pos="); DL_N(dl_magic_position_valid ? 1 : dl_magic_authored_valid ? 2 : 0);
    DL_S(" Aura="); DL_N(dl_owner && dl_word(dl_owner,0x5700)!=0); lengths[0] = n;
    line = dl_probe_lines[1]; n = 0;
    DL_S("Body J/N="); DL_N(dl_probe.materials); DL_S("/"); DL_N(dl_probe.native_materials);
    DL_S("  Albedo="); DL_N(dl_probe.albedo); DL_S("  Views="); DL_N(dl_probe.views); DL_S("  Eyes found/bound="); DL_N(dl_probe.eye_materials); DL_S("/"); DL_N(dl_probe.eyes); lengths[1] = n;
    line = dl_probe_lines[2]; n = 0;
    DL_S("Skip="); DL_S(reasons[dl_probe.reason]); DL_S("  Count="); DL_N(dl_probe.skipped);
    DL_S("  Kept="); DL_N(dl_probe.kept); DL_S("  Lost="); DL_N(dl_probe.lost); DL_S("  Rebound="); DL_N(dl_probe.rebound);
    lengths[2] = n;
    line = dl_probe_lines[3]; n = 0;
    DL_S("Samplers body: "); DL_S(dl_probe.body_samplers[0] ? dl_probe.body_samplers : "?");
    DL_S("  eyes: "); DL_S(dl_probe.eye_samplers[0] ? dl_probe.eye_samplers : "?");
    DL_S(" Fill=");DL_N((u32)(dl_gauge_fraction()*100+0.5f));DL_S("%");
    DL_S(" Full=");DL_N(dl_magic_animation_ok[0]);DL_S("/");DL_N(dl_magic_animation_ok[1]);lengths[3] = n;
    line = dl_probe_lines[4]; n = 0;
    DL_S("Late calls="); DL_N(dl_probe.late_calls); DL_S(" changed="); DL_N(dl_probe.late_changed);
    DL_S(" stale="); DL_N(dl_probe.late_stale); DL_S(" rejected="); DL_N(dl_probe.late_rejected);
    DL_S(" ptr="); n = dl_probe_hex(line, n, dl_probe.late_pointer);
    DL_S(" word="); n = dl_probe_hex(line, n, dl_probe.late_word);
    lengths[4] = n;
#undef DL_S
#undef DL_N
    dl_gauge_probe_hud(list);
    dl_rect(list, 20, 110, 1240, 123, 0x101018ef);
    for (u32 i = 0; i < 5; ++i) dl_text(list, 28, 116 + i * 23, dl_probe_lines[i], lengths[i], 0xffffffff, 17);
}
