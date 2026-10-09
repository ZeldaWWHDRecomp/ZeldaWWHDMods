#pragma once
/* Continue consuming the closing press until its release has reached the game.
 * Otherwise a button held across the panel closing can become a new stock action. */
typedef struct { unsigned blocked; } dragon_dialogue_input;
static inline int dragon_dialogue_quiet(dragon_dialogue_input* state,int consumed,unsigned buttons) {
    const unsigned action_mask=0x8000u|0x4000u|0x0800u; /* VPAD A/B/Left */
    if(consumed){state->blocked=buttons&action_mask;return 1;}
    unsigned previous=state->blocked;
    state->blocked&=buttons;
    return previous!=0;
}

/* The song cancel path closes before the native player executes, just like a panel. */
static inline int dragon_modal_quiet(dragon_dialogue_input* state,int dialog_consumed,
                                    int song_cancelled,unsigned buttons) {
    return dragon_dialogue_quiet(state,dialog_consumed || song_cancelled,buttons);
}
