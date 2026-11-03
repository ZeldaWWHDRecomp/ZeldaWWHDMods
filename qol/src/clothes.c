/* Link's clothes: the story's, the Hero's clothes or the casual clothes (the lobster shirt) at any time.
 * Option "clothes" (Link): "As in the story" (default), "Hero's clothes", "Casual clothes".
 *
 * The casual clothes are Link's model with other textures and a flag (as on the GameCube, decomp
 * daPy_lk_c::playerInit): daPyFlg1_CASUAL_CLOTHES, in HD bit 0x8 of Link + 0x3BC, which also hides his cap and
 * belt buckle. HD keeps the model's clothes texture headers (0x24 bytes each, up to 10) through Link + 0x454 (a
 * list: count at +0, the headers' addresses at +8) and the other clothes' copies at Link + 0x48C; the story's
 * change of clothes swaps the two and flips the flag, and the renderer reads the headers at each draw. This does
 * the same while Link plays (no event) and his clothes are not the chosen ones. A new room makes Link again with
 * the story's clothes: they are changed again at once. Back to "As in the story", the clothes Link was made with
 * come back.
 * Idea from the GreenNaugahyde Android fork (MPL-2.0), mods/clothes.cpp; reimplemented as a guest mod. */
#define CLOTHES_FLAGS 0x3BC
#define CLOTHES_CASUAL 0x8u
#define CLOTHES_LIST 0x454
#define CLOTHES_OTHER 0x48C
#define CLOTHES_HEADER 0x24
#define CLOTHES_MAX 10

static int clothes_choice;   /* 0 the story's, 1 Hero's, 2 casual */
static u32 clothes_link;     /* the Link whose story clothes are known */
static int clothes_story;    /* his clothes as the game made him: 1 casual */

static void clothes_option_changed(void) {
    char text[32];
    text[0] = 0;
    if (!wwhd_config_string("clothes", text, sizeof text)) text[0] = 0;
    clothes_choice = text[0] == 'H' ? 1 : text[0] == 'C' ? 2 : 0;
}

static int clothes_swap(u32 link) {
    u32 list = qol_ld32(link + CLOTHES_LIST);
    if (!qol_guest_ptr(list)) return 0;
    u32 n = qol_ld32(list), heads = qol_ld32(list + 8);
    if (n == 0 || n > CLOTHES_MAX || !qol_guest_ptr(heads)) return 0;
    for (u32 i = 0; i < n; i++) {
        u32 cur = qol_ld32(heads + 4 * i), other = link + CLOTHES_OTHER + i * CLOTHES_HEADER;
        if (!qol_guest_ptr(cur)) continue;
        u8 tmp[CLOTHES_HEADER];
        memmove(tmp, (void*)cur, CLOTHES_HEADER);
        memmove((void*)cur, (void*)other, CLOTHES_HEADER);
        memmove((void*)other, tmp, CLOTHES_HEADER);
    }
    *(volatile u32*)(link + CLOTHES_FLAGS) ^= CLOTHES_CASUAL;
    return 1;
}

/* each of Link's steps */
static void clothes_step(u32 link) {
    const int casual = (qol_ld32(link + CLOTHES_FLAGS) & CLOTHES_CASUAL) != 0;
    if (link != clothes_link) {  /* a new Link (a new room, a load): his clothes are the story's */
        clothes_link = link;
        clothes_story = casual;
    }
    const int want = clothes_choice == 0 ? clothes_story : clothes_choice == 2;
    if (casual == want || qol_event()) return;
    if (clothes_swap(link)) wwhd_log(want ? "clothes: the casual clothes" : "clothes: the Hero's clothes");
}
